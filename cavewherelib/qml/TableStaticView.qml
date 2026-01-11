import QtQuick
import QtQml.Models
import cavewherelib

ListView {
    id: tableViewId

    required property var columnModel // ObjectModel or ListModel

    function calcImplicitWidth() {
        let sum = 0;
        for(let i = 0; i < columnModel.count; i++) {
            let column = columnModel.get(i) as QtObject;
            if(column) {
                sum += column.columnWidth;
            }
        }
        return sum;
    }

    implicitWidth: calcImplicitWidth()

    clip: true
}
