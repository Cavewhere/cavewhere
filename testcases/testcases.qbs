import qbs 1.0
import qbs.TextFile
import qbs.Process
import qbs.FileInfo
import qbs.File

import "../qbsModules/CavewhereApp.qbs" as CavewhereApp

CppApplication {
    name: "cavewhere-test"
    consoleApplication: true

    Depends { name: "Qt"; submodules: ["test"] }
    Depends { name: "dewalls" }
    Depends { name: "cavewhere-lib" }

    cpp.includePaths: ["catch"]

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
            "catch/catch.hpp",
        ]
    }

    Group {
        fileTagsFilter: bundle.isBundle ? ["bundle.content"] : ["application"]
        qbs.install: true
        qbs.installSourceBase: product.buildDirectory
    }
}
