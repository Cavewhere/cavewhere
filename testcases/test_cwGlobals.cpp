//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwGlobals.h"

//Qt includes
#include <QList>
#include <QDir>
#include <QDebug>
#include <QFileInfo>

TEST_CASE("cwGlobals should extract the system paths correctly", "[cwGlobals]") {
    QList<QDir> paths = cwGlobals::systemPaths();
    CHECK(paths.isEmpty() == false);

    for(auto dir : std::as_const(paths)) {
        CHECK(dir.exists() == true);
    }

#ifdef Q_OS_UNIX
    //Check for sensible directories on linux and macos
#ifndef Q_OS_MACOS
    CHECK(paths.contains(QDir("/usr/local/bin")) == true);
#endif
    CHECK(paths.contains(QDir("/usr/bin")) == true);
#endif
}

TEST_CASE("cwGlobals::findExecutable prefers the first survex path that contains cavern", "[cwGlobals]") {
    const QStringList executables = {
#ifdef Q_OS_WIN
        QStringLiteral("cavern.exe"),
#endif
        QStringLiteral("cavern")
    };

    const QList<QDir> paths = cwGlobals::survexPath();
    QList<QString> caverns;

    for (const QDir &directory : paths) {
        for (const QString &executable : executables) {
            const QFileInfo file(directory.absoluteFilePath(executable));
            if (file.exists() && file.isExecutable()) {
                caverns.append(file.absoluteFilePath());
            }
        }
    }

    if (caverns.size() < 2) {
        SKIP("Need at least two survexPath entries containing a cavern executable, install survex on the system");
    }

    const QString expected = caverns.first();
    const QString found = cwGlobals::findExecutable(executables, paths);

    CHECK(found.toStdString() == expected.toStdString());
}
