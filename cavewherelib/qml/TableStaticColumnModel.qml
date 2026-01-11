import QtQuick
import QtQml.Models
import QtQml

Item {
    id: columnWidthModelId
    required property QtObject columnWidth
    required property ListModel columnModel

    function syncWidth(index) {
        if(index < 0 || index >= columnModel.count) {
            return;
        }
        let key = columnModel.get(index).widthName;

        if(key === undefined) {
            console.warn("Can't find key:" + columnModel.get(index).widthName);
            return;
        }

        columnWidth[key] = columnModel.get(index).columnWidth;
    }

    function syncAll() {
        for(let i = 0; i < columnModel.count; i++) {
            syncWidth(i);
        }
    }

    Connections {
        target: columnModel
        function onDataChanged(topLeft, bottomRight, roles) {
            for(let i = topLeft.row; i <= bottomRight.row; i++) {
                columnWidthModelId.syncWidth(i);
            }
        }
    }

    Component.onCompleted: syncAll()
}
