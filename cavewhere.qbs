import qbs 1.0
import qbs.TextFile
import qbs.Process
import qbs.FileInfo
import qbs.File
import "qbsModules/CavewhereApp.qbs" as CavewhereApp

Project {

    property string installDir: {
        if(qbs.targetOS.contains("osx")) {
            return "Cavewhere.app/Contents/MacOS"
        }
        return ""
    }

    references: [
        "survex/survex.qbs",
        "squish/squish.qbs",
        "plotsauce/plotsauce.qbs",
        "QMath3d/QMath3d.qbs",
        "protobuf/protobuf.qbs",
        "zlib/zlib.qbs",
        "installer/installer.qbs",
        "testcases/testcases.qbs"
    ]

    qbsSearchPaths: ["qbsModules"]

    CavewhereApp {
        name: "Cavewhere"

        Group {
            name: "main"
            files: "main.cpp"
        }
    }
}
