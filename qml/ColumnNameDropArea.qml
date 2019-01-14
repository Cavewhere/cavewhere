import QtQuick 2.0

DropArea {
    id: dropAreaId

    property MouseArea dragArea
    property Item delegate
    property var model
    property int indexOffset: 0
    property var transitionsToBlock: []

    property var moveFunction: function(model, oldModel, index, indexOffset, oldIndex) {
    }

    function setTransitionEnable(enabled) {
        for(var index in transitionsToBlock) {
            transitionsToBlock[index].enabled = enabled
        }
    }

    enabled: !dragArea.drag.active

    onEntered: {
        setTransitionEnable(true);
        delegate.dropWidth = drag.source.dragWidth
    }

    onExited: {
        delegate.dropWidth = 0;
    }

    onDropped: {
        var oldModel = dropAreaId.drag.source.model
        var oldIndex = dropAreaId.drag.source.currentIndex

        setTransitionEnable(false);

        drop.accepted = true

        delegate.dropWidth = 0;
        delegate.caught = true;

//        if(model === oldModel && oldIndex < index) {
//            //The column is being move around in the current model.
//            //If the index is large than oldIndex, it'll be off by one because in of the insert
//            oldIndex--;
//        }

        //Update the model with the drop event
        moveFunction(model, oldModel, index, indexOffset, oldIndex);
    }
}
