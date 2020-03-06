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
        "testcases/s3tc-dxt-decompression/s3tc-dxt-decompression.qbs"
    ]

    qbsSearchPaths: ["qbsModules"]

    CavewhereApp {
        name: "CaveWhere"

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

        //Create the plist info for the icon
        bundle.infoPlist: {
            var object = {"CFBundleIconFile":"cavewhereIcon.icns",
                "CFBundleDocumentTypes":[
                    {
                        "CFBundleTypeName": "CaveWhere Project File",
                        "CFBundleTypeExtensions": ["cw"],
                        "CFBundleTypeIconFile": "cavewhereIcon",
                        "LSHandlerRank": "Owner"
                    }
                ]
            }
            return object;
        }
    }
}
