import QtQuick
import cavewherelib

ListView {
    id: tableViewId

    property alias columnModel: columnModelId

    function calcImplicitWidth() {
        let sum = 0;
        for(let i = 0; i < columnModelId.count; i++) {
            let column = columnModelId.get(i) as TableStaticColumn;
            sum += column.columnWidth;
        }
        return sum;
    }

    function updateModel() {
        for(let i = 0; i < columnModelId.count; i++) {
            let column = columnModelId.get(i) as TableStaticColumn
            column.model = model
        }
    }

    implicitWidth: calcImplicitWidth()

    clip: true

    ObjectModel {
        id: columnModelId
        // TableStaticColumn { columnWidth: 200; name: "columnName" }
    }

    onModelChanged: updateModel()
    Component.onCompleted: updateModel()
}
