import qbs 1.0
import qbs.TextFile
import qbs.Process
import qbs.FileInfo
import qbs.File

CppApplication {
    name: "cavewhere-test"
    consoleApplication: true

    Depends { name: "Qt"; submodules: ["test"] }
    Depends { name: "bundle" }
    Depends { name: "dewalls" }
    Depends { name: "cavewhere-lib" }
    Depends { name: "asyncfuture" }
    Depends { name: "s3tc-dxt-decompression" }

    cpp.includePaths: ["catch"]
    cpp.cxxLanguageVersion: "c++17"
//    cpp.treatWarningsAsErrors: true

    qbs.installPrefix: ""

    Properties {
        condition: qbs.targetOS.contains("linux") || qbs.targetOS.contains("macos")
        cpp.cxxFlags: {
            var flags = [
                    ];

            if(qbs.buildVariant == "debug") {
                flags.push("-fsanitize=address");
                flags.push("-fno-omit-frame-pointer");
            }

            return flags;
        }

        cpp.driverFlags: {
            var flags = [];
            if(qbs.buildVariant == "debug") {
                flags.push("-fsanitize=address")
            }
            return flags;
        }
    }

    Group {
        name: "testcases"
        files: [
            "*.cpp",
            "*.h",
            "cavewhere-test.qrc"
        ]
    }

    Group {
        name: "datasets"
        files: [
            "datasets/compass/*",
            "datasets/scrapAutoCalculate/*",
            "datasets/scrapGuessNeighbor/*",
            "datasets/survex/*",
            "datasets/test_cwCSVImporterManager/*"
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
        fileTagsFilter: ["application"]
        qbs.install: true
        qbs.installDir: project.installDir
    }
}
