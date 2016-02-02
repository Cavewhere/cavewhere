import qbs 1.0
import qbs.TextFile

Project {
    name: "installer"

    readonly property string installDir: qbs.installRoot

    qbsSearchPaths: ["../qbsModules"]

    Product {
        name: "cavewhere-install"
        type: "deployedApplication"
        builtByDefault: false

        property var qt: qtApp.Qt.core.binPath

        Depends { name: "Cavewhere" }

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
                            project.installDir + "/" + inputs.application[0].fileName]
                } else if(targetOS.contains("osx")) {
                    deploymentApp = "macdeployqt"

                    var bundlePath = ""

                    //Find the bundle
                    for(var i = 0; i < inputs.installable.length; i++) {
                        print(JSON.stringify(inputs.installable[i].fileTags))
                        print(inputs.installable[i].fileTags.indexOf("bundle"))
                        if(inputs.installable[i].fileTags.indexOf("bundle") != -1) {
                            //Found the bundle
                            var inp = inputs.installable[i];
                            print("Inp:" + JSON.stringify(inp))
                            bundlePath = project.installDir + "/" + inp.fileName;
                            break;
                        }
                    }

                    print("BundlePath:" + bundlePath)

                    if(bundlePath == "") {
                        throw "Bundle not found";
                    }

                    args = [bundlePath,
                            "-qmldir=" + project.installDir + "/Cavewhere.app/Contents/MacOS/qml"
                            ]
                }

                var cmd = new Command(product.qt + "/" + deploymentApp)
                cmd.arguments = args
                cmd.description = "running " + deploymentApp
                return cmd
                //return null
            }
        }
    }

    Product {
        name: "Mac Installer"
        type: "shellInstaller"
        builtByDefault: false
        condition: qbs.targetOS.contains("osx")

        property var qtLibPath: qtApp.Qt.core.libPath
        readonly property string version: Git.productVersion

        Depends { name: "cavewhere-install" }
        Depends { name: "Git" }

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
                filePath: "Cavewhere " + product.version + ".dmg"
                fileTags: "shellInstaller"
            }

            prepare: {
                var cmd = new Command(inputs.shell[0].filePath)
                cmd.arguments = [product.version,
                                 product.sourceDirectory]

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

        readonly property string version: Git.productVersion
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
                arch = "vcredist_x86.exe";
                break;
            case "x86_64":
                arch = "vcredist_x64.exe";
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
                arch = "VC_2013_REDIST_X86_ADD";
                break;
            case "x86_64":
                arch = "VC_2013_REDIST_X64_ADD";
                break;
            default:
                arch = "Unknown Arch";
                break;
            }
            return arch;
        }

        Depends { name: "cavewhere-install" }
        Depends { name: "Git" }

        Group {
            name: "innoFiles"
            fileTags: "innoOriginal"
            files: [
                "windows/cavewhere.iss"
            ]
        }

        Group {
            name: "redist x86"
            fileTags: "redistributableExe"
            files: [
                "windows/vcredist/vcredist_x86.exe"
            ]
            qbs.install: qbs.architecture == "x86"
        }

        Group {
            name: "redist x64"
            fileTags: "redistributableExe"
            files: [
                "windows/vcredist/vcredist_x64.exe"
            ]
            qbs.install: qbs.architecture == "x86_64"
        }

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
#define MyAppVersion "' + product.version + '"\n\
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
                filePath:  "Cavewhere " + product.version + " " + product.arch + ".exe"
                fileTags: "innoInstaller"
            }

            prepare: {
                var cmd = new Command("C:/Program Files (x86)/Inno Setup 5/ISCC.exe")
                cmd.arguments = [input.filePath]
                cmd.description = "running ISCC.exe"
                return cmd
            }
        }
    }

    CppApplication {
        id: qtApp
        name: "QtApp"
        Depends { name: "Qt"; submodules: ["core"] }
    }

}
