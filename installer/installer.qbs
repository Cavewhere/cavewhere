import qbs 1.0
import qbs.TextFile
import "../qbsModules/GitProbe.qbs" as GitProbe

Project {
    name: "installer"

    readonly property string installDir: qbs.installRoot

    qbsSearchPaths: ["../qbsModules"]

    Product {
        name: "cavewhere-install"
        type: "deployedApplication"
        builtByDefault: false

        property var qt: Qt.core.binPath

        Depends { name: "CaveWhere" }
        Depends { name: "cavewhere-test" }
        Depends { name: "Qt.core" }

        Rule {
            multiplex: true
            inputsFromDependencies: ["installable"]

            Artifact {
                filePath: ["sauce"]
                fileTags: ["deployedApplication"]
            }

            prepare: {
                var targetOS = product.moduleProperty("qbs", "targetOS");
                var deploymentApp = "";
                var args = [];
                var description = "";

//                print(JSON.stringify(inputs));

                if(targetOS.contains("windows")) {
                    deploymentApp = "windeployqt.exe"
                    args = ["--qmldir",
                            project.installDir + "/qml",
                            "-sql",
                            "-xml",
                            "-opengl",
                            "-concurrent",
                            "-test",
                            project.installDir + "/" + inputs.application[0].fileName]
                } else if(targetOS.contains("macos")) {
                    deploymentApp = "macdeployqt"

                    var bundleName = "CaveWhere.app"
                    var bundlePath = project.installDir + "/" + bundleName

//                    //Find the bundle
//                    for(var i = 0; i < inputs.installable.length; i++) {
//                        console.error(JSON.stringify(inputs.installable[i].fileTags))
//                        console.error(inputs.installable[i].fileTags.indexOf("bundle.content"))
//                        if(inputs.installable[i].fileTags.indexOf("bundle.content") != -1) {
//                            //Found the bundle
//                            var inp = inputs.installable[i];
////                            console.error("Inp:" + JSON.stringify(inp))
//                            bundlePath = project.installDir + "/" + inp.fileName;
//                            break;
//                        }
//                    }

                    console.error("BundlePath:" + bundlePath)

                    if(bundlePath == "") {
                        throw "Bundle not found";
                    }

                    args = [bundlePath,
                            "-qmldir=" + project.installDir + "/" + bundleName + "/Contents/Resources/qml",
                            "-appstore-compliant"
                            ]
                }

                var cmd = new Command(product.qt + "/" + deploymentApp)
                cmd.arguments = args
                cmd.description = "running " + deploymentApp
                return cmd
            }
        }
    }

    Product {
        name: "Mac Installer"
        type: "shellInstaller"
        builtByDefault: false
        condition: qbs.targetOS.contains("macos")

        property var qtLibPath: Qt.core.libPath
        readonly property string gitVersion: gitMac.productVersion

        Depends { name: "cavewhere-install" }
        Depends { name: "Qt.core" }

        GitProbe {
            id: gitMac
            sourceDirectory: product.sourceDirectory
        }

        Group {
            name: "shellFiles"
            fileTags: "shell"
            files: [
                "mac/installMac.sh"
            ]
        }

        Rule {
            inputs: ["shell"]
            inputsFromDependencies: ["deployedApplication"]
            multiplex: true

            Artifact {
                filePath: "CaveWhere " + product.gitVersion + ".dmg"
                fileTags: "shellInstaller"
            }

            prepare: {
                var cmd = new Command(inputs.shell[0].filePath)
                cmd.arguments = [product.gitVersion,
                                 product.sourceDirectory,
                                 project.codeSign,
                                 project.installDir
                        ]

                cmd.workingDirectory = project.installDir
                cmd.description = "running " + inputs.shell[0].filePath
                return cmd
            }
        }
    }

    Product {
        name: "Windows Installer"
        type: "innoInstaller"
        builtByDefault: false
        condition: qbs.targetOS.contains("windows")

        GitProbe {
            id: git
            sourceDirectory: product.sourceDirectory
        }

        readonly property string gitVersion: git.productVersion
        readonly property string arch: {
            var arch
            switch(qbs.architecture) {
            case "x86":
                arch = "32bit";
                break;
            case "x86_64":
                arch = "64bit";
                break;
            default:
                arch = "Unknown Arch";
                break;
            }
            return arch;
        }
        readonly property string architecturesAllowed: {
            var arch
            switch(qbs.architecture) {
            case "x86":
                arch = "";
                break;
            case "x86_64":
                arch = "x64";
                break;
            default:
                arch = "Unknown Arch";
                break;
            }
            return arch;
        }

        readonly property string architecturesInstallIn64BitMode: {
            var arch
            switch(qbs.architecture) {
            case "x86":
                arch = "";
                break;
            case "x86_64":
                arch = "x64";
                break;
            default:
                arch = "Unknown Arch";
                break;
            }
            return arch;
        }

        readonly property string redistributableExe: {
            var arch
            switch(qbs.architecture) {
            case "x86":
                arch = "vc_redist.x86.exe";
                break;
            case "x86_64":
                arch = "vc_redist.x64.exe";
                break;
            default:
                arch = "Unknown Arch";
                break;
            }
            return arch;
        }

        readonly property string redistributableVersion: {
            var arch
            switch(qbs.architecture) {
            case "x86":
                arch = "VC_2017_REDIST_X86_ADD";
                break;
            case "x86_64":
                arch = "VC_2017_REDIST_X64_ADD";
                break;
            default:
                arch = "Unknown Arch";
                break;
            }
            return arch;
        }

        Depends { name: "cavewhere-install" }

        Group {
            name: "innoFiles"
            fileTags: "innoOriginal"
            files: [
                "windows/cavewhere.iss"
            ]
        }

//        Group {
//            name: "redist x86"
//            fileTags: "redistributableExe"
//            files: [
//                "windows/vcredist/vcredist_x86.exe"
//            ]
//            qbs.install: qbs.architecture == "x86"
//        }

//        Group {
//            name: "redist x64"
//            fileTags: "redistributableExe"
//            files: [
//                "windows/vcredist/vcredist_x64.exe"
//            ]
//            qbs.install: qbs.architecture == "x86_64"
//        }

        Rule {
            inputs: ["innoOriginal"]
            inputsFromDependencies: ["deployedApplication"]
            multiplex: true

            Artifact {
                filePath: "cavewhere-withDefines.iss"
                fileTags: "inno"
            }

            prepare: {
                var cmd = new JavaScriptCommand()
                cmd.description = "Generating inno file " + output.filePath + " from " + inputs.innoOriginal[0].filePath
                cmd.sourceCode = function() {
                    var inputHandle = TextFile(inputs.innoOriginal[0].filePath, TextFile.ReadOnly);
                    var innoInputFile = inputHandle.readAll();
                    inputHandle.close()

                    innoInputFile = '; NOTE: This file was automatically generated by the build process, don\'t edit it\n\n\
#define MyAppName "Cavewhere"\n\
#define MyAppVersion "' + product.gitVersion + '"\n\
#define MyAppPublisher "Cavewhere"\n\
#define MyAppURL "http://www.cavewhere.com"\n\
#define MyAppExeName "Cavewhere.exe"\n\
#define MyArchitecturesAllowed "' + product.architecturesAllowed + '"\n\
#define MyArchitecturesInstallIn64BitMode "' + product.architecturesInstallIn64BitMode + '"\n\
#define Arch "' + product.arch + '"\n\
#define MyRedistributableExe "' + product.redistributableExe + '"\n\
#define MyRedistributableVersion "' + product.redistributableVersion + '"\n\
#define releaseDirectory "' + project.installDir + '"\n\n' +
innoInputFile

                    var outputHandle = TextFile(output.filePath, TextFile.WriteOnly)
                    outputHandle.write(innoInputFile)
                    outputHandle.close()
                }
                return cmd
            }
        }

        Rule {
            inputs: ["inno"]

            Artifact {
                filePath:  "Cavewhere " + product.gitVersion + " " + product.arch + ".exe"
                fileTags: "innoInstaller"
            }

            prepare: {
                var cmd = new Command("C:/Program Files (x86)/Inno Setup 6/ISCC.exe")
                cmd.arguments = [input.filePath]
                cmd.description = "running ISCC.exe"
                return cmd
            }
        }
    }
}
