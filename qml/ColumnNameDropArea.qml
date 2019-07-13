import QtQuick 2.0

DropArea {
    id: dropAreaId

    property MouseArea dragArea
    property Item delegate
    property var model
    property int indexOffset: 0
    property int insertIndex: index

    property var moveFunction: function(model, oldModel, index, indexOffset, oldIndex) {
        console.log("moveFunction not set!")
    }

    enabled: dragArea == null || !dragArea.drag.active

    onEntered: {
        if(delegate) {
            delegate.dropWidth = drag.source.dragWidth
        }
    }

    onExited: {
        if(delegate) {
            delegate.dropWidth = 0;
        }
    }

    onDropped: {
        var oldModel = dropAreaId.drag.source.model
        var oldIndex = dropAreaId.drag.source.currentIndex

        drop.accepted = true

        if(delegate) {
            delegate.dropWidth = 0;
            delegate.caught = true;
        }

        //Update the model with the drop event
        moveFunction(model, oldModel, insertIndex, indexOffset, oldIndex);
    }
}
