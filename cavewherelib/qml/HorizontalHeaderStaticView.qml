pragma ComponentBehavior: Bound

import QtQuick
import cavewherelib

// Qt 6.11.1 broke QC.HorizontalHeaderView when wrapping a list-shaped model:
// only the first column's role data reached the delegate (issue #502). The
// table header is a fixed handful of columns with no resize gesture and no
// syncView, so a Row+Repeater renders the same layout without HeaderView's
// proxy-model indirection.
Item {
    id: horizontalHeader

    required property TableStaticView view
    property alias delegate: repeaterId.delegate
    property alias count: repeaterId.count

    implicitWidth: rowId.implicitWidth
    implicitHeight: rowId.implicitHeight

    function itemAtIndex(index) {
        return repeaterId.itemAt(index)
    }

    Row {
        id: rowId

        Repeater {
            id: repeaterId
            model: horizontalHeader.view.columnModel
        }
    }
}
