import QtQuick
import QtQuick.Controls
import cavewherelib

MenuItem {
    id: root

    property string filePath: ""

    text: {
        if (Qt.platform.os === "osx") {
            return qsTr("Show in Finder");
        } else if (Qt.platform.os === "windows") {
            return qsTr("Show in Explorer");
        }
        return qsTr("Show in File Manager");
    }

    enabled: filePath.length > 0

    onTriggered: {
        if (!filePath || filePath.length === 0) {
            console.warn("RevealInFileManagerMenuItem: filePath is empty");
            return;
        }
        RootData.showInFolder(filePath);
    }
}
