import qbs 1.0
import qbs.TextFile
import qbs.Process
import qbs.FileInfo
import qbs.File
import qbs.Probes
import qbs.Environment

Project {

    //Run the conan probe first, because probes are run in order on how they're found
    Probes.ConanfileProbe {
        id: conan
        conanfilePath: project.sourceDirectory + "/conanfile.py"
        generators: ["qbs", "json"]
        settings: {
            var build_variant = "unknown_build_type"
            if(qbs.buildVariant === "debug") {
                build_variant = "Debug"
            } else if(qbs.buildVariant === "release") {
                build_variant = "Release"
            }

            return ({
                        build_type: build_variant
                    })
        }
    }

    property string conanJsonFile: conan.generatedFilesPath + "/conanbuildinfo.json"

    property string installDir: {
        if(qbs.targetOS.contains("macos")) {
            return "Cavewhere.app/Contents/MacOS"
        }
        return ""
    }

    property string codeSign
//    property var conanJson: conanJsonProbe.conanJson



//    Probe {
//        id: conanJsonProbe
//        property var conanJson: {}
//        property string conanfile: project.sourceDirectory + "/conanbuildinfo.json"
//        configure: {
//            var fileHandle = TextFile(conanfile, TextFile.ReadOnly)
//            var jsonText = fileHandle.readAll();
//            conanJson = JSON.parse(jsonText);
//        }
//    }

    references: [
        "src/cavewhereLib.qbs",
//        "survex/survex.qbs",
        "squish/squish.qbs",
//        "plotsauce/plotsauce.qbs",
        "QMath3d/QMath3d.qbs",
        //"protobuf/protobuf.qbs",
        //"zlib/zlib.qbs",
        "installer/installer.qbs",
        "testcases/testcases.qbs",
        //"dewalls/dewalls.qbs",
        "qt-qml-models/QtQmlModels.qbs",
        "asyncfuture/asyncfuture.qbs",
        "autoBuild/autoBuild.qbs",
        "testcases/s3tc-dxt-decompression/s3tc-dxt-decompression.qbs",
        "testcases/combiner/combiner.qbs",
        conan.generatedFilesPath + "/conanbuildinfo.qbs"
    ]

    qbsSearchPaths: ["qbsModules"]

    Application {
        id: applicationId
        name: "CaveWhere"

        readonly property string appPrefix: {
            if(qbs.targetOS.contains("macos")) {
                return name + ".app/Contents/MacOS/"
            }
            return ""
        }
        readonly property string resourcesPrefix: {
            if(qbs.targetOS.contains("macos")) {
                return name + ".app/Contents/Resources/"
            }
            return appPrefix
        }

        property string prefix: project.sourceDirectory

        Depends { name: "cpp" }
        Depends { name: "cavewhere-lib" }
        Depends { name: "Qt"
            submodules: [ "core",
                "gui",
                "widgets",
                "quick",
                "sql",
                "test",
            ]
        }
        Depends { name: "libqtqmltricks-qtqmlmodels" }
        Depends { name: "bundle" }
//        Depends { name: "survex" }
        Depends { name: "cavewhere-test" }
        Depends { name: "ConanEnvironment" }

        ConanEnvironment.conanJsonFile: conanJsonFile

//        cpp.setupRunEnviroment: {
//            var env = new Environment.getEnv("PATH"); //, qbs.pathListSeparator, true);
////            env.append("C:\tacos");
////            env.set();
//            console.error("I get here!" + env)
//        }

//        setupRunEnvironment: {
//            console.error("I get here!");
//        }

        consoleApplication: {
            if(qbs.targetOS.contains("windows")) {
                return qbs.buildVariant == "debug"
            }
            return false
        }

        cpp.cxxLanguageVersion: "c++17"
        cpp.treatWarningsAsErrors: false
        cpp.minimumMacosVersion: "10.14"

        cpp.includePaths: [
            applicationId.prefix + "/src",
            applicationId.prefix + "/src/utils",
            buildDirectory + "/serialization",
            buildDirectory + "/versionInfo"
        ]

        qbs.installPrefix: ""

        Group {
            name: "main"
            files: [
                "main.cpp"
            ]
        }

        Group {
            name: "conan"
            files: [
                "conanfile.py"
            ]
        }

        Group {
            name: "README"
            files: [
                "README.md"
            ]
        }

        Group {
            name: "autoBuild"
            files: [
                ".travis.yml",
            ]
        }

        Group {
            name: "asan"
            qbs.install: qbs.targetOS.contains("macos") || qbs.targetOS.contains("linux")
            files: [
                "asan/suppressedLeaks.txt",
            ]
        }

        //Create the plist info for the icon
        bundle.identifierPrefix: "com.cavewhere"
        bundle.infoPlist: {
            var object = {"CFBundleIconFile":"cavewhereIcon.icns",
                "CFBundleDocumentTypes":[
                    {
                        "CFBundleTypeName": "CaveWhere Project File",
                        "CFBundleTypeExtensions": ["cw"],
                        "CFBundleTypeIconFile": "cavewhereIcon",
                        "LSHandlerRank": "Owner"
                    }
                ],
                "CFBundleShortVersionString":"0.09",
                "CFBundleVersion":"0.0.09"
            }
            return object;
        }

        Properties {
            condition: qbs.targetOS.contains("macos")

            cpp.frameworks: [
                "OpenGL"
            ]
        }

        Properties {
            condition: qbs.targetOS.contains("macos") || qbs.targetOS.contains("linux")
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

        Properties {
            condition: qbs.targetOS.contains("linux")
            cpp.dynamicLibraries: [
                "GL"
            ]
            cpp.libraryPaths: [
                "/usr/lib/x86_64-linux-gnu/mesa/"
            ]
        }

        Properties {
            condition: qbs.targetOS.contains("windows")

            cpp.cxxFlags: [
    //            "/WX", //Treat warnings as errors
                "-D_SCL_SECURE_NO_WARNINGS", //Ignore warning from protobuf
            ]

            cpp.dynamicLibraries: [
                "OpenGL32"
            ]
        }

        Group {
            name: "qmlFiles"
            qbs.install: qbs.buildVariant == "release" && !product.consoleApplication
            qbs.installDir: product.resourcesPrefix + "qml"
            files: [
                applicationId.prefix + "/qml/*.qml",
                applicationId.prefix + "/qml/*.js"
            ]
        }

        Group {
            name: "shaderFiles"
            qbs.installDir: product.resourcesPrefix + "shaders"
            qbs.install: qbs.buildVariant == "release" && !product.consoleApplication

            files: [
                applicationId.prefix + "/shaders/*.vert",
                applicationId.prefix + "/shaders/*.frag",
                applicationId.prefix + "/shaders/*.geam",
                applicationId.prefix + "/shaders/*.vsh",
                applicationId.prefix + "/shaders/*.fsh"
            ]
        }

        Group {
            name: "shaderFiles-compass"
            qbs.installDir: product.resourcesPrefix + "shaders/compass"
            qbs.install: qbs.buildVariant == "release" && !product.consoleApplication

            files: [
                applicationId.prefix + "/shaders/compass/*.vsh",
                applicationId.prefix + "/shaders/compass/*.fsh"
            ]
        }

        Group {
            name: "packageCreatorScripts"

            files: [
                applicationId.prefix + "/installer/mac/installMac.sh"
            ]
        }



        Group {
            name: "DocumentationFiles"
            files: [
                applicationId.prefix + "/docs/FileFormatDocumentation.txt",
                applicationId.prefix + "/LICENSE.txt",
            ]
        }

        Group {
            name: "rcFiles"
            files: [
                applicationId.prefix + "/Cavewhere.rc"
            ]
        }

        Group {
            name: "qrcFiles"
            files: [
                applicationId.prefix + "/resources.qrc"
            ]
        }

        Group {
            name: "macIcons"
            files: [
                applicationId.prefix + "/cavewhereIcon.icns",
            ]
            qbs.install: true
            qbs.installDir: product.resourcesPrefix
        }

        Group {
            fileTagsFilter: bundle.isBundle ? ["bundle.content"] : ["application"]
            qbs.install: true
            qbs.installSourceBase: product.buildDirectory
        }
    }
}
