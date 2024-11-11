import QtQuick
import cavewherelib

ListView {
    id: tableViewId

    property alias columnModel: columnModelId

    function calcImplicitWidth() {
        let sum = 0;
        for(let i = 0; i < columnModelId.count; i++) {
            sum += columnModelId.get(i).columnWidth;
        }
        return sum;
    }

    implicitWidth: calcImplicitWidth()

    clip: true

    ObjectModel {
        id: columnModelId
        // TableStaticColumn { columnWidth: 200; name: "columnName" }
    }
}
