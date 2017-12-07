/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include "quotainfo.h"
#include "account.h"
#include "accountstate.h"
#include "networkjobs.h"
#include "folderman.h"
#include "creds/abstractcredentials.h"
#include <theme.h>

#include <QTimer>

namespace OCC {

namespace {
    static const int defaultIntervalT = 30 * 1000;
    static const int failIntervalT = 5 * 1000;
}

QuotaInfo::QuotaInfo(AccountState *accountState, QObject *parent)
    : QObject(parent)
    , _accountState(accountState)
    , _lastQuotaTotalBytes(0)
    , _lastQuotaUsedBytes(0)
    , _active(false)
{
    connect(accountState, &AccountState::stateChanged,
        this, &QuotaInfo::slotAccountStateChanged);
    connect(&_jobRestartTimer, &QTimer::timeout, this, &QuotaInfo::slotCheckQuota);
    _jobRestartTimer.setSingleShot(true);
}

void QuotaInfo::setActive(bool active)
{
    _active = active;
    slotAccountStateChanged();
}


void QuotaInfo::slotAccountStateChanged()
{
    if (canGetQuota()) {
        auto elapsed = _lastQuotaRecieved.msecsTo(QDateTime::currentDateTime());
        if (_lastQuotaRecieved.isNull() || elapsed >= defaultIntervalT) {
            slotCheckQuota();
        } else {
            _jobRestartTimer.start(defaultIntervalT - elapsed);
        }
    } else {
        _jobRestartTimer.stop();
    }
}

void QuotaInfo::slotRequestFailed()
{
    _lastQuotaTotalBytes = 0;
    _lastQuotaUsedBytes = 0;
    _jobRestartTimer.start(failIntervalT);
}

bool QuotaInfo::canGetQuota() const
{
    if (!_accountState || !_active) {
        return false;
    }
    AccountPtr account = _accountState->account();
    return _accountState->isConnected()
        && account->credentials()
        && account->credentials()->ready();
}

QString QuotaInfo::quotaBaseFolder() const
{
    return Theme::instance()->quotaBaseFolder();
}

void QuotaInfo::slotCheckQuota()
{
    if (!canGetQuota()) {
        return;
    }

    if (_job) {
        // The previous job was not finished?  Then we cancel it!
        _job->deleteLater();
    }

    AccountPtr account = _accountState->account();
    _job = new PropfindJob(account, quotaBaseFolder(), this);
    _job->setProperties(QList<QByteArray>() << "quota-available-bytes"
                                            << "quota-used-bytes");
    connect(_job.data(), &PropfindJob::result, this, &QuotaInfo::slotUpdateLastQuota);
    connect(_job.data(), &AbstractNetworkJob::networkError, this, &QuotaInfo::slotRequestFailed);
    _job->start();
}

void QuotaInfo::slotUpdateLastQuota(const QVariantMap &result)
{
    // The server can return fractional bytes (#1374)
    // <d:quota-available-bytes>1374532061.2</d:quota-available-bytes>
    qint64 avail = result["quota-available-bytes"].toDouble();
    _lastQuotaUsedBytes = result["quota-used-bytes"].toDouble();
    // negative value of the available quota have special meaning (#3940)
    _lastQuotaTotalBytes = avail >= 0 ? _lastQuotaUsedBytes + avail : avail;
    emit quotaUpdated(_lastQuotaTotalBytes, _lastQuotaUsedBytes);
    _jobRestartTimer.start(defaultIntervalT);
    _lastQuotaRecieved = QDateTime::currentDateTime();
}
}
