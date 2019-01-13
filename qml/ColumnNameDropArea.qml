import QtQuick 2.0

DropArea {
    id: insertLeftDropArea

    property MouseArea dragArea
    property Item delegate
    property var model
    property int indexOffset: 0

    property var transitionsToBlock: []

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
        setTransitionEnable(false);
        drop.accepted = true

        delegate.dropWidth = 0;

        delegate.caught = true;

        var oldModel = insertLeftDropArea.drag.source.model
        var oldIndex = insertLeftDropArea.drag.source.currentIndex

        //Update the model with the drop event
        var value = oldModel.get(oldIndex);
        oldModel.remove(oldIndex);
        model.insert(index + indexOffset, value);
    }
}
