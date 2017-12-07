/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef MIRALL_FOLDERWATCHER_H
#define MIRALL_FOLDERWATCHER_H

#include "config.h"

#include <QList>
#include <QLoggingCategory>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QHash>
#include <QScopedPointer>
#include <QSet>

class QTimer;

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcFolderWatcher)

class FolderWatcherPrivate;
class Folder;

/**
 * @brief Monitors a directory recursively for changes
 *
 * Folder Watcher monitors a directory and its sub directories
 * for changes in the local file system. Changes are signalled
 * through the pathChanged() signal.
 *
 * Note that if new folders are created, this folderwatcher class
 * does not automatically add them to the list of monitored
 * dirs. That is the responsibility of the user of this class to
 * call addPath() with the new dir.
 *
 * @ingroup gui
 */

class FolderWatcher : public QObject
{
    Q_OBJECT
public:
    /**
     * @param root Path of the root of the folder
     */
    FolderWatcher(const QString &root, Folder *folder = 0L);
    virtual ~FolderWatcher();

    /**
     * Not all backends are recursive by default.
     * Those need to be notified when a directory is added or removed while the watcher is disabled.
     * This is a no-op for backends that are recursive
     */
    void addPath(const QString &);
    void removePath(const QString &);

    /* Check if the path is ignored. */
    bool pathIsIgnored(const QString &path);

    /**
     * Returns false if the folder watcher can't be trusted to capture all
     * notifications.
     *
     * For example, this can happen on linux if the inotify user limit from
     * /proc/sys/fs/inotify/max_user_watches is exceeded.
     */
    bool isReliable() const;

signals:
    /** Emitted when one of the watched directories or one
     *  of the contained files is changed. */
    void pathChanged(const QString &path);

    /**
     * Emitted if some notifications were lost.
     *
     * Would happen, for example, if the number of pending notifications
     * exceeded the allocated buffer size on Windows. Note that the folder
     * watcher could still be able to capture all future notifications -
     * i.e. isReliable() is orthogonal to losing changes occasionally.
     */
    void lostChanges();

protected slots:
    // called from the implementations to indicate a change in path
    void changeDetected(const QString &path);
    void changeDetected(const QStringList &paths);

protected:
    QHash<QString, int> _pendingPathes;

private:
    QScopedPointer<FolderWatcherPrivate> _d;
    QTime _timer;
    QSet<QString> _lastPaths;
    Folder *_folder;
    bool _isReliable = true;

    friend class FolderWatcherPrivate;
};
}

#endif
