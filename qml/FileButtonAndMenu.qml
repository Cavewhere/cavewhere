// import QtQuick 1.0 // to target S60 5th Edition or Maemo 5
import QtQuick 2.0

Rectangle {
    id: fileMenuButton

    property var terrainRenderer; //For taking screenshots
    property DataMainPage dataPage;

    width: 100
    height: Math.max(caveIcon.height, textItem.height)

    anchors.top: parent.top
    anchors.left: parent.left

    Image {
        id: caveIcon
        source: "qrc:/icons/cave.png"
        anchors.left: parent.left
    }

    Text {
        id: textItem
        anchors.left: caveIcon.right
        anchors.leftMargin: 3

        text: "Cavewhere"
    }

    MouseArea {
        anchors.fill: parent
        onClicked: fileMenu.popupOnTopOf(fileMenuButton, 0, fileMenuButton.height)
    }

    ContextMenu {
        id: fileMenu

        MenuItem {
            text: "New"
            onTriggered:{
                dataPage.resetSideBar(); //Fixes a crash when a new project is loaded
                project.newProject();
            }
        }

        MenuItem {
            text: "Save"
            onTriggered: project.save();
        }

        MenuItem {
            text: "Save As"
            onTriggered: project.saveAs();
        }

        MenuItem {
            text: "Load"
            onTriggered: {
                dataPage.resetSideBar() //Fixes a crash when a new project is loaded
                project.load();
            }
        }

        MenuItem {
            text: "Compute Scraps"
            onTriggered: scrapManager.updateAllScraps()
        }

        MenuItem {
            text: "Screenshot"
            onTriggered: {

                var exporter = Qt.createQmlObject('import QtQuick 2.0; import Cavewhere 1.0; ExportRegioonViewerToImageTask {}', fileMenuButton, "");
                exporter.window = quickWindow;
                exporter.regionViewer = terrainRenderer;
                exporter.takeScreenshot();
                exporter.destroy();
            }
        }

        MenuItem {
            text: "Quit"
            onTriggered: Qt.quit()
        }
    }
}
