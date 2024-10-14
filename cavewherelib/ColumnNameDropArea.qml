import QtQuick 2.0 as QQ

QQ.DropArea {
    id: dropAreaId

    property QQ.MouseArea dragArea
    property QQ.Item delegate
    property var model
    property int indexOffset: 0
    required property int index
    property int insertIndex: index

    property var moveFunction: function(model, oldModel, index, indexOffset, oldIndex) {
        console.log("moveFunction not set!")
    }

    enabled: dragArea == null || !dragArea.drag.active

    onEntered: (drag) => {
        if(delegate) {
            delegate.dropWidth = (drag.source as ColumnNameDelegate).dragWidth
        }
    }

    onExited: {
        if(delegate) {
            delegate.dropWidth = 0;
        }
    }

    onDropped: (drop) => {
        let srcDelegate = (drop.source) as ColumnNameDelegate
        var oldModel = srcDelegate.model
        var oldIndex = srcDelegate.currentIndex

        drop.accepted = true

        if(delegate) {
            delegate.dropWidth = 0;
            delegate.caught = true;
        }

        //Update the model with the drop event
        moveFunction(model, oldModel, insertIndex, indexOffset, oldIndex);
    }
}
