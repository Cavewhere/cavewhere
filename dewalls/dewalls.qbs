import qbs 1.0

Project {
    name: "dewalls"

    StaticLibrary {
        name: "dewalls"
        Depends { name: "cpp" }
        Depends { name: "Qt"; submodules: ["core"] }

        cpp.includePaths: ["src"]

        Export {
            Depends { name: "cpp" }
            cpp.includePaths: ["src"]
        }

        files: [
            "src/*.cpp",
            "src/*.h",
        ]
    }

    CppApplication {
        name: "dewalls-test"
        consoleApplication: true
        type: "application"

        Depends { name: "cpp" }

        Depends { name: "Qt"; submodules: ["core", "testlib"] }

        cpp.includePaths: ["src"]

        files: [
            "main.cpp",
            "src/*.cpp",
            "src/*.h",
        ]
    }
}
