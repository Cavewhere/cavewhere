import qbs 1.0
import qbs.TextFile
import qbs.Process
import qbs.FileInfo
import qbs.File

/**
  The CavewhereApp is used by both cavewhere.qbs and testcases/testcases.qbs.
  cavewhere.qbs creates the Cavewhere application where as testcases/testcases.qbs
  creates a test case application bases on all the cavewhere classes. This type
  (CavewhereApp) shares the same logic build between cavewhere.qbs and
  testcases/testcases.qbs
  */
Application {
    id: applicationId

    readonly property string installPrefix: {
        if(qbs.targetOS.contains("osx")) {
            return name + ".app/Contents/MacOS/"
        }
        return ""
    }
    property string prefix: sourceDirectory.indexOf("/testcases") > 0 ? "../" : ""

    Depends { name: "cpp" }
    Depends { name: "Qt";
        submodules: [ "core",
            "gui",
            "widgets",
            "quick",
            "sql",
            "opengl",
            "xml",
            "concurrent" ]
    }
    Depends { name: "cavewhere-lib" }
    Depends { name: "sdk-utilities" }

    cpp.includePaths: [
        applicationId.prefix + "src",
        applicationId.prefix + "src/utils",
        buildDirectory + "/serialization",
        buildDirectory + "/versionInfo"
    ]

    cpp.rpaths: [
        Qt.core.libPath
    ]

    Properties {
        condition: qbs.targetOS.contains("osx")

        cpp.dynamicLibraries: [
            "c++"
        ]

        cpp.frameworks: [
            "OpenGL"
        ]
    }

    Properties {
        condition: qbs.targetOS.contains("osx")
        cpp.cxxFlags: {
            var flags = [
                        "-stdlib=libc++", //Needed for protoc
                        "-std=c++11", //For c++11 support
                        "-Werror", //Treat warnings as errors

                    ];

            if(qbs.buildVariant == "debug") {
                flags.push(["-fsanitize=address",
                            "-fno-omit-frame-pointer"]);
            }

            return flags;
        }

        cpp.linkerFlags: {
            var flags = [];
            if(qbs.buildVariant == "debug") {
                flags.push("-fsanitize=address")
            }
            return flags;
        }
    }

    Properties {
        condition: qbs.targetOS.contains("linux")
        cpp.cxxFlags: [
            "-std=c++11", //For c++11 support
            "-Werror" //Treat warnings as errors
        ]
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
            "/WX", //Treat warnings as errors
            "-D_SCL_SECURE_NO_WARNINGS", //Ignore warning from protobuf
        ]

        cpp.dynamicLibraries: [
            "OpenGL32"
        ]
    }

    cpp.minimumOsxVersion: "10.7"

    Group {
        fileTagsFilter: ["bundle"]
        qbs.install: true
    }

    Group {
        fileTagsFilter: ["application"]
        qbs.installDir: qbs.targetOS.contains("darwin") ? "Cavewhere.app/Contents/MacOS" : ""
        qbs.install: true
    }

    Group {
        name: "qmlFiles"
        qbs.install: qbs.buildVariant == "release"
        qbs.installDir: product.installPrefix + "qml"
        files: [
            applicationId.prefix + "qml/*.qml",
            applicationId.prefix + "qml/*.js"
        ]
    }

    Group {
        name: "shaderFiles"
        qbs.installDir: product.installPrefix + "shaders"
        qbs.install: qbs.buildVariant == "release"

        files: [
            applicationId.prefix + "shaders/*.vert",
            applicationId.prefix + "shaders/*.frag",
            applicationId.prefix + "shaders/*.geam",
            applicationId.prefix + "shaders/*.vsh",
            applicationId.prefix + "shaders/*.fsh"
        ]
    }

    Group {
        name: "shaderFiles-compass"
        qbs.installDir: product.installPrefix + "shaders/compass"
        qbs.install: qbs.buildVariant == "release"

        files: [
            applicationId.prefix + "shaders/compass/*.vsh",
            applicationId.prefix + "shaders/compass/*.fsh"
        ]
    }

    Group {
        name: "packageCreatorScripts"

        files: [
            applicationId.prefix + "installer/mac/installMac.sh"
        ]
    }



    Group {
        name: "DocumentationFiles"
        files: [
            applicationId.prefix + "docs/FileFormatDocumentation.txt",
            applicationId.prefix + "LICENSE.txt",
        ]
    }

    Group {
        name: "rcFiles"
        files: [
            applicationId.prefix + "Cavewhere.rc"
        ]
    }

    Group {
        name: "qrcFiles"
        files: [
            applicationId.prefix + "resources.qrc"
        ]
    }

    Group {
        name: "macIcons"
        files: [
            applicationId.prefix + "cavewhereIcon.icns",
        ]
        fileTags: ["icns-in"]
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
            Qt.core.binPath + "/Qt5Testd.dll",
            Qt.core.binPath + "/icuin*.dll",
            Qt.core.binPath + "/icuuc*.dll",
            Qt.core.binPath + "/icudt*.dll"
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
            Qt.core.binPath + "/Qt5Test.dll",
            Qt.core.binPath + "/icuin*.dll",
            Qt.core.binPath + "/icuuc*.dll",
            Qt.core.binPath + "/icudt*.dll",

            //Redistrobution libraries for vs2010 32bit
            "c:/windows/system32/MSVCR120.DLL",
            "c:/windows/system32/MSVCP120.DLL"
        ]

    }


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
