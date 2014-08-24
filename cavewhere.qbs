import qbs 1.0
import qbs.TextFile
import qbs.Process
import qbs.FileInfo
import qbs.File
import "cavewhereBuildFunctions.js" as utils

Project {
    Application {
        id: applicationId
        name: "Cavewhere"

        property string git: utils.findIfExisting(["C:/Program Files/Git/cmd/git.cmd",
                                             "/usr/bin/git",
                                             "/usr/local/bin/git"])

        property string protoc: utils.findIfExisting(["/usr/local/bin/protoc",
                                                     windowsProtoPath + "/Release/protoc.exe",
                                                     windowsProtoPath + "/protoc.exe"])

        property string windowsSquishPath: "C:/windowsBuild/libs/win32/squish-1.11/squish-1.11"
        property string windowsProtoPath: "C:/windowsBuild/libs/win32/protobuf-2.5.0rc1/vsprojects"
        property string windowsZlibPath: "C:/windowsBuild/libs/win32/zlib-1.2.5"
        property var sharedIncludes: [
            "src",
            "QMath3d",
            "src/utils",
            product.buildDirectory + "/serialization",
            product.buildDirectory + "/versionInfo"
        ]

        Depends { name: "cpp" }
        Depends { name: "Qt";
            submodules: [ "core",
                "gui",
                "widgets",
                "script",
                "quick",
                "sql",
                "opengl",
                "xml",
                "concurrent" ]
        }
        Depends { name: "QMath3d" }
//        Depends { name: "icns-out" }

        Qt.quick.qmlDebugging: true //qbs.buildVariant === "debug"


        Properties {
            condition: qbs.targetOS.contains("osx")

            cpp.includePaths: sharedIncludes.concat("/usr/local/include")

            cpp.libraryPaths: [
                "/usr/local/lib"
            ]

            cpp.dynamicLibraries: [
                "squish",
                "z",
                "protobuf",
                "c++"
            ]

            cpp.frameworks: [
                "OpenGL"
            ]

            cpp.cxxFlags: [
                "-stdlib=libc++", //Needed for protoc
                "-Werror", //Treat warnings as errors
                "-std=c++11" //For c++11 support
            ]
        }

        Properties {
            condition: qbs.targetOS.contains("windows")

            cpp.includePaths: sharedIncludes.concat([windowsProtoPath + "/include",
                                                   windowsSquishPath,
                                                   windowsZlibPath])
            cpp.cxxFlags: [
                "/WX", //Treat warnings as errors
                "-D_SCL_SECURE_NO_WARNINGS", //Ignore warning from protobuf
            ]
        }

        Properties {
            condition: qbs.targetOS.contains("windows") &&
                       qbs.buildVariant.contains("debug")

            cpp.libraryPaths: [
                windowsSquishPath + "/lib/vs9",
                windowsZlibPath,
                windowsProtoPath + "/Debug"
            ]

            cpp.dynamicLibraries: [
                "squishd",
                "zlibd",
                "libprotobuf",
                "OpenGL32"
            ]
        }

        Properties {
            condition: qbs.targetOS.contains("windows") &&
                       qbs.buildVariant.contains("release")

            cpp.libraryPaths: [
                windowsSquishPath + "/lib/vs9",
                windowsZlibPath,
                windowsProtoPath + "/Release"
            ]

            cpp.dynamicLibraries: [
                "squish",
                "zlib",
                "libprotobuf",
                "OpenGL32"
            ]
        }

        cpp.defines:[
            "TRILIBRARY",
            "ANSI_DECLARATORS"
        ]

        Properties {
            //This property is set so we can debug QML will in the application in
            //debug mode.
            condition: qbs.buildVariant == "debug"
            cpp.defines: outer.concat("CAVEWHERE_SOURCE_DIR=\"" + sourceDirectory + "\"")
        }

        Properties {
            condition: qbs.buildVariant == "release"
            cpp.defines: outer.concat("CAVEWHERE_SOURCE_DIR=\"\"")
        }

        cpp.infoPlistFile: "Info.plist"
        cpp.minimumOsxVersion: "10.7"

        Group {
            fileTagsFilter: ["application", "applicationbundle"]
            qbs.install: true
        }

        Group {
            name: "ProtoFiles"

            files: [
                "src/cavewhere.proto",
                "src/qt.proto"
            ]

            fileTags: ["proto"]
        }

        Group {
            name: "cppFiles"
            files: [
                "src/*.cpp",
                "src/*.h",
                "src/utils/*.cpp",
                "src/utils/*.h"
            ]
        }

        Group {
            name: "qmlFiles"
            qbs.install: qbs.buildVariant == "release"
            qbs.installDir: "qml"
            files: [
                "qml/*.qml",
                "qml/*.js"
            ]
        }

        Group {
            name: "shaderFiles"
            qbs.installDir: "shaders"
            qbs.install: qbs.buildVariant == "release"

            files: [
                "shaders/*.vert",
                "shaders/*.frag",
                "shaders/*.geam",
                "shaders/*.vsh",
                "shaders/*.fsh"
            ]
        }

        Group {
            name: "shaderFiles-compass"
            qbs.installDir: "shaders/compass"
            qbs.install: qbs.buildVariant == "release"

            files: [
                "shaders/compass/*.vsh",
                "shaders/compass/*.fsh"
            ]
        }

        Group {
            name: "uiForms"
            files: [
                "src/cwImportSurvexDialog.ui",
                "src/cwTaskProgressDialog.ui"
            ]
        }

        Group {
            name: "packageCreatorScripts"

            files: [
                "installer/mac/installMac.sh"
            ]
        }



        Group {
            name: "DocumentationFiles"
            files: [
                "docs/FileFormatDocumentation.txt",
                "LICENSE.txt"
            ]
        }

        Group {
            name: "rcFiles"
            files: [
                "Cavewhere.rc"
            ]
        }

        Group {
            name: "qrcFiles"
            files: [
                "resources.qrc"
            ]
        }

        Group {
            name: "macIcons"
            files: [
                "cavewhereIcon.icns",
            ]
            fileTags: ["icns-in"]
        }

        Group {
            name: "survexDepends"
            condition: qbs.targetOS == "osx"
            files: [
                "plotsauce",
                "cavern",
                "share"
            ]
            fileTags: ["survex"]
        }

        Group {
            name: "windowsDLLs-debug"
            condition: qbs.targetOS == "windows" && qbs.buildVariant == "debug"
            qbs.install: true
            files:[
                Qt.core.binPath + "/Qt5Concurrentd.dll",
                Qt.core.binPath + "/Qt5Cored.dll",
                Qt.core.binPath + "/Qt5Guid.dll",
                Qt.core.binPath + "/Qt5OpenGLd.dll",
                Qt.core.binPath + "/Qt5Qmld.dll",
                Qt.core.binPath + "/Qt5Quickd.dll",
                Qt.core.binPath + "/Qt5Sqld.dll",
                Qt.core.binPath + "/Qt5Widgetsd.dll",
                Qt.core.binPath + "/Qt5Networkd.dll",
                Qt.core.binPath + "/Qt5Xmld.dll",
                Qt.core.binPath + "/icuin*.dll",
                Qt.core.binPath + "/icuuc*.dll",
                Qt.core.binPath + "/icudt*.dll",
                windowsZlibPath + "/zlibd1.dll"
            ]
        }

        Group {
            name: "windowsDLLs-release"
            condition: qbs.targetOS == "windows" && qbs.buildVariant == "release"
            qbs.install: true
            files:[
                Qt.core.binPath + "/Qt5Concurrent.dll",
                Qt.core.binPath + "/Qt5Core.dll",
                Qt.core.binPath + "/Qt5Gui.dll",
                Qt.core.binPath + "/Qt5OpenGL.dll",
                Qt.core.binPath + "/Qt5Qml.dll",
                Qt.core.binPath + "/Qt5Quick.dll",
                Qt.core.binPath + "/Qt5Sql.dll",
                Qt.core.binPath + "/Qt5Widgets.dll",
                Qt.core.binPath + "/Qt5Network.dll",
                Qt.core.binPath + "/Qt5Xml.dll",
                Qt.core.binPath + "/icuin*.dll",
                Qt.core.binPath + "/icuuc*.dll",
                Qt.core.binPath + "/icudt*.dll",
                windowsZlibPath + "/zlib1.dll"
            ]

        }

//        Group {
//            name: "windowsPlatform"
//            condition: qbs.targetOS == "windows" && qbs.buildVariant == "release"
//            qbs.install: true
//            qbs.installDir: "platforms"
//            files: [Qt.core.pluginPath + "/platforms/qwindows.dll"]
//        }

//        Group {
//            name: "windowsImageFormats"
//            condition: qbs.targetOS == "windows" && qbs.buildVariant == "release"
//            qbs.install: true
//            qbs.installDir: "imageformats"
//            files: [Qt.core.pluginPath + "/imageformats/*.dll"]
//            excludeFiles: [
//                Qt.core.pluginPath + "/imageformats/*d.dll" //Exclude all debug dlls for images
//            ]
//        }

//        Group {
//            name: "windowsSQLDrivers"
//            condition: qbs.targetOS == "windows" && qbs.buildVariant == "release"
//            qbs.install: true
//            qbs.installDir: "sqldrivers"
//            files: [Qt.core.pluginPath + "/sqldrivers/qsqlite.dll"]
//        }

//        Group {
//            name: "macInfo"
//            files: [
//                "Info.plist"
//            ]
//        }

        Rule {
            id: macIconCopier
            inputs: ["icns-in"]
            auxiliaryInputs: ["application"]

            Artifact {
                fileTags: ["resourcerules"]
                filePath: product.buildDirectory + "/Cavewhere.app/Contents/Resources/" + FileInfo.baseName(input.filePath) + ".icns"
//                fileName: applicationId.name + ".app/Contents/Resources/" + FileInfo.baseName(input.filePath) + ".icns"
            }

            prepare: {
                print("Preparing" + input.filePath + " to " + output.filePath)
                var cp = "/bin/cp"
                var realOutputFile = product.buildDirectory + "/Cavewhere.app/Contents/Resources/" + FileInfo.baseName(input.filePath) + ".icns"
                var cmd = new Command(cp,
                                      [input.filePath, realOutputFile])
                cmd.description = "Copying icons to resources " + input.filePath + "to" + output.filePath
                cmd.highlight = 'codegen'
                return cmd
            }
        }

        Rule {
            id: survexCopier
            inputs: ["survex"]
            auxiliaryInputs: ["application"]

            Artifact {
                fileTags: ["resourcerules"]
                filePath: product.buildDirectory + "/Cavewhere.app/Contents/MacOS/" + FileInfo.baseName(input.filePath)
//                fileName: applicationId.name + ".app/Contents/Resources/" + FileInfo.baseName(input.filePath) + ".icns"
            }

            prepare: {
                print("Preparing" + input.filePath + " to " + output.filePath)
                var cp = "/bin/cp"
                var realOutputFile = product.buildDirectory + "/Cavewhere.app/Contents/MacOS/" + FileInfo.baseName(input.filePath)
                var cmd = new Command(cp,
                                      ["-r", input.filePath, realOutputFile])
                cmd.description = "Copying survex " + input.filePath + "to" + output.filePath
                cmd.highlight = 'codegen'
                return cmd
            }
        }

        Rule {
            id: protoCompiler
            inputs: ["proto"]

            Artifact {
                fileTags: ["hpp"]
                filePath: "serialization/" + FileInfo.baseName(input.filePath) + ".pb.h"
            }

            Artifact {
                fileTags: ["cpp"]
                filePath: "serialization/" + FileInfo.baseName(input.filePath) + ".pb.cc"
            }

            prepare: {
                var protoc = product.protoc
                var proto_path = FileInfo.path(input.filePath)
                var cpp_out = product.buildDirectory + "/serialization"

                var protoPathArg = "--proto_path=" + proto_path
                var cppOutArg = "--cpp_out=" + cpp_out

                var cmd = new Command(protoc,
                                      [protoPathArg, cppOutArg, input.filePath])
                cmd.description = "Running protoc on " + input.filePath + "with args " + protoPathArg + " " + cppOutArg
                cmd.highlight = 'codegen'
                return cmd;
            }
        }

        Transformer {
            id: cavewhereVersionGenerator

            Artifact {
                fileTags: ["hpp"]
                filePath: "versionInfo/cavewhereVersion.h"
            }

            prepare: {
                var cmd = new JavaScriptCommand();
                cmd.description = "generating version info in" + output.filePath;

                //Use git to query the version
                var git = product.git
                var gitDescribe = "Unknown Version"

                if(File.exists(git)) {
                    var gitProcess = new Process();
                    gitProcess.setWorkingDirectory(product.sourceDirectory)
                    gitProcess.exec(git, ["describe"] ,true);
                    gitDescribe = gitProcess.readStdOut();
                    gitDescribe = gitDescribe.replace(/(\r\n|\n|\r)/gm,""); //Remove newlines
                }

                cmd.cavewhereVersion = gitDescribe

                cmd.sourceCode = function() {
                    var all = "#ifndef cavewhereVersion_H\n #define cavewhereVersion_H\n static const QString CavewhereVersion = \"" + cavewhereVersion + "\";\n #endif\n\n";
                    var file = new TextFile(output.filePath, TextFile.WriteOnly);
                    file.write(all);
                    file.close();
                }
                return cmd;
            }
        }


    }


    DynamicLibrary {
        name: "QMath3d"
        Depends { name: "cpp" }
        Depends { name: "Qt";
            submodules: [ "core", "gui" ]
        }

        cpp.defines: ["Q_MATH_3D"]

        Group {
            fileTagsFilter: ["dynamiclibrary"]
            qbs.install: true
        }

        files: [
            "QMath3d/smallqt3d_global.h",
            "QMath3d/qbox3d.h",
            "QMath3d/qplane3d.h",
            "QMath3d/qray3d.h",
            "QMath3d/qsphere3d.h",
            "QMath3d/qtriangle3d.h",
            "QMath3d/qbox3d.cpp",
            "QMath3d/qplane3d.cpp",
            "QMath3d/qray3d.cpp",
            "QMath3d/qsphere3d.cpp",
            "QMath3d/qtriangle3d.cpp",
        ]
    }
}
