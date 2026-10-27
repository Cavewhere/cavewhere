/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwFileRevealer.h"

//Qt includes
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

namespace {
#if defined(Q_OS_WIN) && QT_CONFIG(process)
    //Explorer selects the file it is given when handed "/select,".
    const auto kWindowsFileManagerTool = QStringLiteral("explorer");
    const auto kWindowsSelectFlag = QStringLiteral("/select,");
#elif defined(Q_OS_MAC) && QT_CONFIG(process)
    //The macOS launcher; "-R" reveals the file in Finder rather than opening it.
    const auto kMacOpenTool = QStringLiteral("/usr/bin/open");
    const auto kMacRevealFlag = QStringLiteral("-R");
#endif
}

cwFileRevealer::cwFileRevealer(QObject* parent) :
    QObject(parent)
{
}

QString cwFileRevealer::resolvedPath(const QString& directory,
                                     const QString& relativePath)
{
    if(relativePath.isEmpty()) {
        return QString();
    }

    if(QFileInfo(relativePath).isAbsolute()) {
        return QDir::cleanPath(relativePath);
    }

    if(directory.isEmpty()) {
        return QString();
    }

    return QDir::cleanPath(QDir(directory).absoluteFilePath(relativePath));
}

void cwFileRevealer::showInFileManager(const QString& absolutePath)
{
    if(absolutePath.isEmpty()) {
        return;
    }

    const QFileInfo info(absolutePath);

    if(!info.exists()) {
        //Selecting a file that is gone leaves the file manager silent, so
        //show the folder that would hold it instead.
        const QDir containingDirectory(info.absolutePath());
        if(containingDirectory.exists()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(containingDirectory.absolutePath()));
        }
        return;
    }

#if defined(Q_OS_WIN) && QT_CONFIG(process)
    QStringList arguments;
    if(!info.isDir()) {
        arguments << kWindowsSelectFlag;
    }
    arguments << QDir::toNativeSeparators(absolutePath);
    if(QProcess::startDetached(kWindowsFileManagerTool, arguments)) {
        return;
    }
#elif defined(Q_OS_MAC) && QT_CONFIG(process)
    if(QProcess::startDetached(kMacOpenTool, {kMacRevealFlag, absolutePath})) {
        return;
    }
#endif

    QDesktopServices::openUrl(
        QUrl::fromLocalFile(info.isDir() ? absolutePath : info.absolutePath()));
}

void cwFileRevealer::openFile(const QString& absolutePath)
{
    if(absolutePath.isEmpty()) {
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(absolutePath));
}
