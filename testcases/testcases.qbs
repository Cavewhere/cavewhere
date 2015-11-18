import qbs 1.0
import qbs.TextFile
import qbs.Process
import qbs.FileInfo
import qbs.File

import "../qbsModules/CavewhereApp.qbs" as CavewhereApp

CavewhereApp {
    name: "cavewhere-test"
    consoleApplication: true

    Depends { name: "Qt"; submodules: ["test"] }

    Group {
        name: "testcases"
        files: [
            "*.cpp",
            "*.h"
        ]
    }

    Group {
        name: "CatchTestLibrary"
        files: [
            "catch.hpp"
        ]
    }
}
