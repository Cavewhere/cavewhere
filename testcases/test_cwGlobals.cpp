//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwGlobals.h"

//Qt includes
#include <QList>
#include <QDir>
#include <QDebug>

TEST_CASE("cwGlobals should extract the system paths correctly", "[cwGlobals]") {
    QList<QDir> paths = cwGlobals::systemPaths();
    CHECK(paths.isEmpty() == false);

    for(auto dir : paths) {
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
