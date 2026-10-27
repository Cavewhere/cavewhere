/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWFILEREVEALER_H
#define CWFILEREVEALER_H

//Our includes
#include "cwGlobals.h"

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QString>

/**
 * Hands a file on disk to the desktop: reveals it in the system file
 * manager (Finder on macOS, the containing folder elsewhere) or opens it
 * with the handler the system associates with its type.
 *
 * Path resolution is separate from both: resolvedPath() turns a
 * directory plus a path relative to it into one absolute path and
 * touches nothing on disk, so a caller holding a project-relative name
 * (an external centerline's entry file, relative to its attachment
 * directory) can resolve first and act second.
 *
 * A QML singleton — it carries no state, and every caller wants the same
 * two verbs.
 */
class CAVEWHERE_LIB_EXPORT cwFileRevealer : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FileRevealer)
    QML_SINGLETON

public:
    explicit cwFileRevealer(QObject* parent = nullptr);

    // Absolute path of relativePath inside directory, cleaned of "." and
    // ".." segments. An absolute relativePath is returned as it stands,
    // since it already names the file. Empty when relativePath is empty,
    // or when a relative one has no directory to resolve against.
    Q_INVOKABLE static QString resolvedPath(const QString& directory,
                                            const QString& relativePath);

    // Selects absolutePath in the system file manager — Finder on macOS,
    // Explorer on Windows. Where selecting a file is unavailable, or the
    // file is gone, the containing folder opens.
    Q_INVOKABLE static void showInFileManager(const QString& absolutePath);

    // Opens absolutePath with the system's default handler for its type.
    Q_INVOKABLE static void openFile(const QString& absolutePath);
};

#endif // CWFILEREVEALER_H
