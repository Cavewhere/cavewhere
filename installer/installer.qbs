import qbs 1.0

Project {
    name: "installer"


    Product {
        name: "cavewhere-install"
        type: "deployedApplication"

        readonly property string installDir: "../../build-cavewhere-" +
                                             profile.replace("qtc_", "") + "-" + qbs.buildVariant + "/" +
                                             profile + "-" + qbs.buildVariant + "/" +
                                             "install-root"
        property var qt: qtApp.Qt.core.binPath

        Group {
            name: "Cavewhere Executable"
            fileTags: ["qtApplication"]
            files: [
                installDir + "/Cavewhere.exe"
            ]
        }

        Rule {
            inputs: ["qtApplication"]

            Artifact {
                filePath: ["sauce"]
                fileTags: ["deployedApplication"]
            }

            prepare: {
                var cmd = new Command(product.qt + "/windeployqt.exe")
                cmd.workDirectory = product.installDir
                cmd.arguments = ["--qmldir",
                                 "qml",
                                 input.filePath]
                cmd.description = "running windeployqt.exe"
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
