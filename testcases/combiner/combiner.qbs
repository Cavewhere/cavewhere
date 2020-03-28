import qbs 1.0

CppApplication {
    name: "combiner-test"
    consoleApplication: true

    Depends { name: "Qt.core" }
    Depends { name: "bundle" }
    Depends { name: "cavewhere-test" }
    Depends { name: "asyncfuture" }

    cpp.cxxLanguageVersion: "c++17"
    qbs.installPrefix: ""

    Group {
        name: "combiner"
        files: [
            "*.cpp",
            "*.h",
        ]
    }

    Group {
        fileTagsFilter: ["application"]
        qbs.install: true
        qbs.installDir: project.installDir
    }

}
