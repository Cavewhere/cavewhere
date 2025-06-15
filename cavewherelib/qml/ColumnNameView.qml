pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Layouts
import cavewherelib

QQ.Item {
    id: root

    property ColumnNameModel model
    property bool isDragging
    property var moveFunction: function(model, oldModel, index, indexOffset, oldIndex) {
        var value = oldModel.get(oldIndex);
        oldModel.remove(oldIndex);
        model.insert(index + indexOffset, value);
    }

    implicitWidth: Math.max(layoutId.width, root.Layout.minimumWidth)
    implicitHeight: layoutId.height

    QQ.Row {
        id: layoutId

        move:  QQ.Transition {
            id: rowTransitionId
            enabled: false
            QQ.NumberAnimation { properties: "x"; }
        }

        QQ.Repeater {
            id: repeaterId
            model: root.model
            delegate: ColumnNameDelegate {
                model: root.model
                moveFunction: root.moveFunction
            }
        }
    }

    ColumnNameDropArea {

        anchors.left: layoutId.right
        anchors.right: parent.right

        insertIndex: repeaterId.count
        implicitHeight: layoutId.height

        model: root.model
        moveFunction: root.moveFunction
        index: -1 //FIXME: not sure in -1 is correct
    }
}
