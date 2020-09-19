import qbs 1.0
import qbs.TextFile
import qbs.Process
import qbs.FileInfo
import qbs.File
import "../qbsModules/GitProbe.qbs" as GitProbe

/**
  The cavewhere-lib is used by both cavewhere.qbs and testcases/testcases.qbs.
  This library prevents building cavewhere source twice when building object
  files for the application and for the testcases.
  */
DynamicLibrary {
    id: cavewhereLibId

    name: "cavewhere-lib"

    //For mac os x we need to build dylib instead of framework bundle. When running
    //macdepolyqt for release, with a framework, an extra "lib" is added to the
    //path which prevents macdeployqt from finding the correct library's location
//    consoleApplication: true

    readonly property string gitVersion: git.productVersion

    //This is a work around for debug QtPdf on windows to work
    readonly property bool win32QPDFExists: {
        return File.exists(Qt.core.binPath + "/Qt5Pdfd.dll")

    }
    //    readonly property string rpath: buildDirectory


    Depends { name: "cpp" }
    Depends { name: "Qt";
        submodules: [ "core",
            "gui",
            "widgets",
            "quick",
            "sql",
            "opengl",
            "xml",
            "concurrent",
            "svg",
            //"pdf" //This doesn't work under debug
        ]
    }

    Depends { name: "QMath3d" }
    Depends { name: "squish" }
    Depends { name: "plotsauce" }
    Depends { name: "protoc" }
    Depends { name: "protobuf" }
    Depends { name: "z" }
    Depends { name: "dewalls" }
    Depends { name: "libqtqmltricks-qtqmlmodels" }
    Depends { name: "asyncfuture" }
    Depends { name: "s3tc-dxt-decompression" }
    Depends { name: "bundle" }

    Depends {
        name: "Qt.pdf"
        required: false
    }

    GitProbe {
        id: git
        sourceDirectory: product.sourceDirectory
    }

    Export {
        Depends { name: "cpp" }
//        cpp.rpaths: [product.rpath]
        cpp.includePaths: [
            ".",
            "utils",
            product.buildDirectory + "/versionInfo"
        ]

        Depends { name: "Qt";
            submodules: [ "core",
                "gui",
                "widgets",
                "quick",
                "sql",
                "opengl",
                "xml",
                "concurrent",
                "svg",
            ]
        }

        Depends { name: "libqtqmltricks-qtqmlmodels" }
        Depends { name: "protobuf" }
        Depends { name: "QMath3d" }
    }

    Group {
        fileTagsFilter: ["dynamiclibrary"]
        condition: qbs.buildVariant == "release"
        qbs.install: qbs.targetOS.contains("windows")
    }

//    cpp.rpaths: [Qt.core.libPath]
    cpp.cxxLanguageVersion: "c++17"
    cpp.treatWarningsAsErrors: false
    Qt.quick.compilerAvailable: false

    cpp.includePaths: [
        ".",
        "utils",
        product.buildDirectory + "/serialization",
        product.buildDirectory + "/versionInfo"
    ]

    Properties {
        condition: qbs.targetOS.contains("macos")

        cpp.sonamePrefix: qbs.installRoot + "/lib"

        cpp.frameworks: [
            "OpenGL"
        ]
    }

    Group {
        fileTagsFilter: ["bundle.content"]
        qbs.install: bundle.isBundle
        qbs.installSourceBase: product.buildDirectory
        qbs.installDir: "lib"
        qbs.installPrefix: ""
    }

    Properties {
        condition: qbs.targetOS.contains("macos")
        cpp.cxxFlags: {
            var flags = [];

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

        cpp.dynamicLibraries: {
            var base = [
                        "OpenGL32",
                    ]
            if(qbs.buildVariant == "debug" && cavewhereLibId.win32QPDFExists) {
                base.push("Qt5Pdfd")
            }
            return base
        }

        //Manually add QPdf on windows for debugging to work
        cpp.includePaths: {
            if(qbs.buildVariant == "debug" && cavewhereLibId.win32QPDFExists) {
                return outer.concat(
                            Qt.core.incPath + "/QtPdf"
                            )
            }
            return outer;
        }

    }

    Group {
        name: "Windows QPDF Install"
        condition: qbs.targetOS.contains("windows") && Qt.pdf.present

        qbs.install: qbs.buildVariant == "release"

        files: [
            Qt.core.binPath + "/Qt5Pdf.dll"
        ]
    }

    cpp.defines: {
        var base = ["TRILIBRARY",
                    "ANSI_DECLARATORS"];

        if(qbs.buildVariant == "debug") {
            base = base.concat("CAVEWHERE_SOURCE_DIR=\"" + sourceDirectory + "/.." + "\"");
            base = base.concat("CW_DEBUG");
        } else if(qbs.buildVariant == "release") {
            base = base.concat("CAVEWHERE_SOURCE_DIR=\"\"");
        }

        if(qbs.targetOS.contains("windows")) {
            base = base.concat('CAVEWHERE_LIB')
        }

        console.error("cavewhereLibId:" + cavewhereLibId.win32QPDFExists)

        if(Qt.pdf.present || cavewhereLibId.win32QPDFExists) {
            base = base.concat('WITH_PDF_SUPPORT')
        }

        return base;
    }

    Group {
        name: "ProtoFiles"

        files: [
            "cavewhere.proto",
            "qt.proto"
        ]

        fileTags: ["proto"]
    }

    Group {
        name: "cppFiles"
        files: [
            "*.cpp",
            "*.h",
            "utils/*.cpp",
            "utils/*.h"
        ]
    }

    Group {
        name: "uiForms"
        files: [
            "cwImportTreeDataDialog.ui",
            "cwTaskProgressDialog.ui",
        ]
    }

    Group {
        name: "qrcFiles"
        files: [
            "../resources.qrc",
            "../fonts/fonts.qrc"
        ]
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

    Rule {
        id: cavewhereVersionGenerator

        multiplex: true

        Artifact {
            fileTags: ["hpp"]
            filePath: "versionInfo/cavewhereVersion.h"
        }

        prepare: {
            var cmd = new JavaScriptCommand();
            cmd.description = "generating version info in" + output.filePath;

            //Use git to query the version
            cmd.cavewhereVersion = product.gitVersion

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
