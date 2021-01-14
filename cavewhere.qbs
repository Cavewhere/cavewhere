import qbs 1.0
import qbs.TextFile
import qbs.Process
import qbs.FileInfo
import qbs.File

Project {

    property string installDir: {
        if(qbs.targetOS.contains("macos")) {
            return "Cavewhere.app/Contents/MacOS"
        }
        return ""
    }

    property string codeSign

    references: [
        "src/cavewhereLib.qbs",
        "survex/survex.qbs",
        "squish/squish.qbs",
        "plotsauce/plotsauce.qbs",
        "QMath3d/QMath3d.qbs",
        "protobuf/protobuf.qbs",
        "zlib/zlib.qbs",
        "installer/installer.qbs",
        "testcases/testcases.qbs",
        "dewalls/dewalls.qbs",
        "qt-qml-models/QtQmlModels.qbs",
        "asyncfuture/asyncfuture.qbs",
        "autoBuild/autoBuild.qbs",
        "testcases/s3tc-dxt-decompression/s3tc-dxt-decompression.qbs",
        "testcases/combiner/combiner.qbs"
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
        Depends { name: "Qt"
            submodules: [ "core",
                "gui",
                "widgets",
                "quick",
                "sql",
                "test"
            ]
        }
        Depends { name: "cavewhere-lib" }
        Depends { name: "libqtqmltricks-qtqmlmodels" }
        Depends { name: "bundle" }
        Depends { name: "cavern" }
        Depends { name: "cavewhere-test" }

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

//        cpp.rpaths: [
//            Qt.core.libPath
//        ]

        qbs.installPrefix: ""

        Group {
            name: "main"
            files: [
                "main.cpp"
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

        Rule {
            id: protoCompiler
            inputs: ["proto"]
            inputsFromDependencies: ["application"]
            multiplex: true

            outputArtifacts: {
                var artifacts = [];

                for(var i in inputs.proto) {
                    var baseName = FileInfo.baseName(inputs.proto[i].filePath)
                    var fullPath = "serialization/" + baseName
                    var headerPath = fullPath + ".pb.h"
                    var srcPath = fullPath + ".pb.cc"

                    var headerArtifact = { filePath: headerPath, fileTags: ["hpp"] }
                    var srcArtifact = { filePath: srcPath, fileTags: ["cpp"] }

                    artifacts.push(headerArtifact)
                    artifacts.push(srcArtifact)
                }

                return artifacts
            }

            outputFileTags: ["hpp", "cpp"]

            prepare: {
                var protoc = "protoc-not-found"

                for(var i in inputs.application) {
                    if(inputs.application[i].filePath.contains("protoc")) {
                        protoc = inputs.application[i].filePath
                    }
                }

                var commands = [];
                for(var i in inputs.proto) {
                    var proto_path = FileInfo.path(inputs.proto[i].filePath)
                    var cpp_out = product.buildDirectory + "/serialization"

                    var protoPathArg = "--proto_path=" + proto_path
                    var cppOutArg = "--cpp_out=" + cpp_out
                    var inputFile = inputs.proto[i].filePath

                    var cmd = new Command(protoc,
                                          [protoPathArg, cppOutArg, inputFile])
                    cmd.description = "Running protoc on " + inputFile + "with args " + protoc + " " + protoPathArg + " " + cppOutArg
                    cmd.highlight = 'codegen'

                    commands.push(cmd)
                }
                return commands;
            }
        }
    }

    Product {
        name: "vcpkg"
        type: ["application"]

        Group {
            files: ["vcpkg/bootstrap-vcpkg.bat"]
            fileTags: ["vcpkg-bootstrap"]
        }

        Rule {
            inputs: ["vcpkg-boostrap"]
            Artifact {
                filePath: "vcpkg/vcpkg.exe"
                fileTags: ["application"]
            }
            prepare: {
                var cmd = new Command("bootstrap-vcpkg.bat");
                cmd.description = "bootstrapping vcpkg";
                cmd.workingDirectory = product.sourceDirectory + "/vcpkg";
                return [cmd];
            }
        }
    }

    Product {
        name: "proj4"
        type: ["dynamiclibrary"]

        Depends { name: "vcpkg" }
        Depends { name :"cpp" }

        Export {
            Depends { name: "cpp" }
            cpp.includePaths: [
                "vcpkg/packages/proj4_x64-windows/include"
            ]
            cpp.libraryPaths: [
                "vcpkg/packages/proj4_x64-windows/debug/lib"
            ]
            cpp.dynamicLibraries: [
                "proj_d"
            ]
        }

        Rule {
            inputs: []
            multiplex: true

            Artifact {
                fileTags: ["dynamiclibrary"]
            }
            prepare: {
                var cmd = new Command("vcpkg.exe", ["install", "proj4:x64-windows"]);
                cmd.description = "fetching and installing proj4";
                cmd.workingDirectory = product.sourceDirectory + "/vcpkg";
                return [cmd];
            }
        }
    }

    Product {
        name: "vpckgDlls"

        Depends {
            name: "proj4"
        }

        Group {
            name: "proj4-windows-debug-dlls"
            files: [
                "vcpkg/packages/libjpeg-turbo_x64-windows/debug/bin/turbojpeg.dll",
                "vcpkg/packages/libjpeg-turbo_x64-windows/debug/bin/jpeg62.dll",
                "vcpkg/packages/sqlite3_x64-windows/debug/bin/sqlite3.dll",
                "vcpkg/packages/liblzma_x64-windows/debug/bin/lzmad.dll",
                "vcpkg/packages/tiff_x64-windows/debug/bin/tiffd.dll",
                "vcpkg/packages/zlib_x64-windows/debug/bin/zlibd1.dll",
                "vcpkg/packages/proj4_x64-windows/debug/bin/proj_d.dll"
            ]

            qbs.install: true
        }

        /* This needs to be replace the enviromental variable PROJ_LIB=D:\docs\Projects\cavewhere\cavewhere\vcpkg\packages\proj4_x64-windows\share\proj4
        Group {
            name: "proj4-database"
            files: [
                "vcpkg/packages/proj4_x64-windows/share/proj4/proj.db"
            ]
            qbs.install: true
        }
        */


    }

}
