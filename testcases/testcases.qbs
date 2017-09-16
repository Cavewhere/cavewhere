import qbs 1.0
import qbs.TextFile
import qbs.Process
import qbs.FileInfo
import qbs.File

import "../qbsModules/CavewhereApp.qbs" as CavewhereApp

Project {

    CavewhereApp {
        name: "cavewhere-test"
        consoleApplication: true

        Depends { name: "Qt"; submodules: ["testlib"] }
        Depends { name: "dewalls" }

        Group {
            name: "testcases"
            files: [
                "*.cpp",
                "*.h",
                "cavewhere-test.qrc"
            ]
        }

        Group {
            name: "dewalls testcases"
            files: [
                "../dewalls/test/*.cpp",
                "../dewalls/test/*.h",
                "../dewalls/test/dewalls-test.qrc"
            ]
            excludeFiles: "../dewalls/test/dewallstests.cpp"
        }

        Group {
            name: "CatchTestLibrary"
            files: [
                "catch.hpp",
            ]
        }
    }

    CppApplication {
        name: "cavewhere-qml-test"
        consoleApplication: true

        Depends { name: "Qt"; submodules: ["testlib", "quick", "qmltest"] }

        Group {
            fileTagsFilter: ["application"]
//            qbs.installDir: qbs.targetOS.contains("darwin") ? "Cavewhere.app/Contents/MacOS" : ""
            qbs.install: true
        }

        files: [
            "qml/*.cpp"
        ]
    }
}
