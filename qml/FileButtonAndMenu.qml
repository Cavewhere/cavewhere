// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0
import QtQuick.Controls 1.0

MenuBar {
    property var terrainRenderer; //For taking screenshots
    property DataMainPage dataPage;

    Menu {
        title: "File"

        MenuItem {
            text: "New"
            shortcut: "Ctrl+N"
            onTriggered:{
                dataPage.resetSideBar(); //Fixes a crash when a new project is loaded
                project.newProject();
            }
        }

        MenuItem {
            text: "Save"
            shortcut: "Ctrl+S"
            onTriggered: project.save();
        }

        MenuItem {
            text: "Save As"
            onTriggered: project.saveAs();
        }

        MenuItem {
            text: "Open"
            shortcut: "Ctrl+O"
            onTriggered: {
                dataPage.resetSideBar() //Fixes a crash when a new project is loaded
                project.load();
            }
        }

        MenuItem {
            visible: Qt.platform.os === "linux" || Qt.platform.os === "windows"
            text: "Quit"
            shortcut: "Ctrl+Q"
            onTriggered: Qt.quit()
        }
    }

    Menu {
        title: "Debug"

        MenuItem {
            text: "Screenshot"
            onTriggered: {

                var exporter = Qt.createQmlObject('import QtQuick 2.0; import Cavewhere 1.0; ExportRegioonViewerToImageTask {}', fileMenuButton, "");
                exporter.window = quickView;
                exporter.regionViewer = terrainRenderer;

                //Hide all children
                var i;
                for(i in terrainRenderer.children) {
                    terrainRenderer.children[i].visible = false
                }

                exporter.takeScreenshot();
                exporter.destroy();

                //Show all Children
                for(i in terrainRenderer.children) {
                    terrainRenderer.children[i].visible = true
                }
            }
        }

        MenuItem {
            text: "Compute Scraps"
            onTriggered: scrapManager.updateAllScraps()
        }

        MenuItem {
            text: "Reload"
            shortcut: "Ctrl+R"
            onTriggered: qmlReloader.reload();
        }
    }

}

//Rectangle {
//    id: fileMenuButton



//    width: 100
//    height: Math.max(caveIcon.height, textItem.height)

//    anchors.top: parent.top
//    anchors.left: parent.left

//    Image {
//        id: caveIcon
//        source: "qrc:/icons/cave.png"
//        anchors.left: parent.left
//    }

//    Text {
//        id: textItem
//        anchors.left: caveIcon.right
//        anchors.leftMargin: 3

//        text: "Cavewhere"
//    }

//    MouseArea {
//        anchors.fill: parent
//        onClicked: fileMenu.popupOnTopOf(fileMenuButton, 0, fileMenuButton.height)
//    }

//    ContextMenu {
//        id: fileMenu

//        MenuItem {
//            text: "New"
//            onTriggered:{
//                dataPage.resetSideBar(); //Fixes a crash when a new project is loaded
//                project.newProject();
//            }
//        }

//        MenuItem {
//            text: "Save"
//            onTriggered: project.save();
//        }

//        MenuItem {
//            text: "Save As"
//            onTriggered: project.saveAs();
//        }

//        MenuItem {
//            text: "Load"
//            onTriggered: {
//                dataPage.resetSideBar() //Fixes a crash when a new project is loaded
//                project.load();
//            }
//        }

//        MenuItem {
//            text: "Compute Scraps"
//            onTriggered: scrapManager.updateAllScraps()
//        }

//        MenuItem {
//            text: "Screenshot"
//            onTriggered: {

//                var exporter = Qt.createQmlObject('import QtQuick 2.0; import Cavewhere 1.0; ExportRegioonViewerToImageTask {}', fileMenuButton, "");
//                exporter.window = quickView;
//                exporter.regionViewer = terrainRenderer;

//                //Hide all children
//                var i;
//                for(i in terrainRenderer.children) {
//                    terrainRenderer.children[i].visible = false
//                }

//                exporter.takeScreenshot();
//                exporter.destroy();

//                //Show all Children
//                for(i in terrainRenderer.children) {
//                    terrainRenderer.children[i].visible = true
//                }
//            }
//        }

//        MenuItem {
//            text: "Reload"
//            onTriggered: qmlReloader.reload();
//        }

//        MenuItem {
//            text: "Quit"
//            onTriggered: Qt.quit()
//        }
//    }
//}
