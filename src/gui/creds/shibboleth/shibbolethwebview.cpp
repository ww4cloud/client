/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QApplication>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QWebFrame>
#include <QWebPage>
#include <QMessageBox>
#include <QNetworkReply>
#include <QSettings>
#include <QMainWindow>

#include "creds/shibboleth/shibbolethwebview.h"
#include "creds/shibbolethcredentials.h"
#include "account.h"
#include "logger.h"
#include "accessmanager.h"
#include "theme.h"
#include "configfile.h"
#include "cookiejar.h"

namespace {
const char ShibbolethWebViewGeometryC[] = "ShibbolethWebView/Geometry";
}

namespace OCC {

class UserAgentWebPage : public QWebPage
{
public:
    UserAgentWebPage(QObject *parent)
        : QWebPage(parent)
    {
        if (!qEnvironmentVariableIsEmpty("OWNCLOUD_SHIBBOLETH_DEBUG")) {
            settings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
        }
    }
    QString userAgentForUrl(const QUrl &url) const
    {
        return QWebPage::userAgentForUrl(url) + " " + Utility::userAgentString();
    }
};

ShibbolethWebView::ShibbolethWebView(AccountPtr account, QWidget *parent)
    : QWebView(parent)
    , _account(account)
    , _accepted(false)
    , _cursorOverriden(false)
{
    // no minimize
    setWindowFlags(Qt::Dialog);
    setAttribute(Qt::WA_DeleteOnClose);

    QWebPage *page = new UserAgentWebPage(this);
    connect(page, &QWebPage::loadStarted,
        this, &ShibbolethWebView::slotLoadStarted);
    connect(page, &QWebPage::loadFinished,
        this, &ShibbolethWebView::slotLoadFinished);

    // Make sure to accept the same SSL certificate issues as the regular QNAM we use for syncing
    QObject::connect(page->networkAccessManager(), &QNetworkAccessManager::sslErrors,
        _account.data(), &Account::slotHandleSslErrors);

    // The Account keeps ownership of the cookie jar, it must outlive this webview.
    account->lendCookieJarTo(page->networkAccessManager());
    connect(static_cast<CookieJar *>(page->networkAccessManager()->cookieJar()), &CookieJar::newCookiesForUrl,
        this, &ShibbolethWebView::onNewCookiesForUrl);

    page->mainFrame()->load(account->url());
    this->setPage(page);
    setWindowTitle(tr("%1 - Authenticate").arg(Theme::instance()->appNameGUI()));

    // Debug view to display the cipher suite
    if (!qEnvironmentVariableIsEmpty("OWNCLOUD_SHIBBOLETH_DEBUG")) {
        // open an additional window to display some cipher debug info
        QWebPage *debugPage = new UserAgentWebPage(this);
        debugPage->mainFrame()->load(QUrl("https://cc.dcsec.uni-hannover.de/"));
        QWebView *debugView = new QWebView(this);
        debugView->setPage(debugPage);
        QMainWindow *window = new QMainWindow(this);
        window->setWindowTitle(tr("SSL Chipher Debug View"));
        window->setCentralWidget(debugView);
        window->show();
    }
    // If we have a valid cookie, it's most likely expired. We can use this as
    // as a criteria to tell the user why the browser window pops up
    QNetworkCookie shibCookie = ShibbolethCredentials::findShibCookie(_account.data(), ShibbolethCredentials::accountCookies(_account.data()));
    if (shibCookie != QNetworkCookie()) {
        Logger::instance()->postOptionalGuiLog(tr("Reauthentication required"), tr("Your session has expired. You need to re-login to continue to use the client."));
    }

    ConfigFile config;
    QSettings settings(config.configFile());
    resize(900, 700); // only effective the first time, later overridden by restoreGeometry
    restoreGeometry(settings.value(ShibbolethWebViewGeometryC).toByteArray());
}

ShibbolethWebView::~ShibbolethWebView()
{
    ConfigFile config;
    QSettings settings(config.configFile());
    settings.setValue(ShibbolethWebViewGeometryC, saveGeometry());
}

void ShibbolethWebView::onNewCookiesForUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url)
{
    if (url.host() == _account->url().host()) {
        QNetworkCookie shibCookie = ShibbolethCredentials::findShibCookie(_account.data(), cookieList);
        if (shibCookie != QNetworkCookie()) {
            Q_EMIT shibbolethCookieReceived(shibCookie);
            accept();
            close();
        }
    }
}

void ShibbolethWebView::closeEvent(QCloseEvent *event)
{
    if (_cursorOverriden) {
        QApplication::restoreOverrideCursor();
    }

    if (!_accepted) {
        Q_EMIT rejected();
    }
    QWebView::closeEvent(event);
}

void ShibbolethWebView::slotLoadStarted()
{
    if (!_cursorOverriden) {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        _cursorOverriden = true;
    }
}

void ShibbolethWebView::slotLoadFinished(bool success)
{
    if (_cursorOverriden) {
        QApplication::restoreOverrideCursor();
    }

    if (!title().isNull()) {
        setWindowTitle(QString::fromLatin1("%1 - %2 (%3)").arg(Theme::instance()->appNameGUI(), title(), url().host()));
    }

    if (!success) {
        qCWarning(lcShibboleth) << "Could not load Shibboleth login page to log you in.";
    }
}

void ShibbolethWebView::accept()
{
    _accepted = true;
}

} // namespace OCC
