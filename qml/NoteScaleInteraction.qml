import QtQuick 2.0
import Cavewhere 1.0

DrawLengthInteraction {
    id: noteScaleInteraction

    property NoteTransform noteTransform
    property Note note

    doneTextLabel: "<b>In cave length</b>"

    onDoneButtonPressed: {
        var imageSize = imageItem.imageProperties.size
        var scale = noteTransform.calculateScale(firstMouseLocation,
                                                 secondMouseLocation,
                                                 lengthObject,
                                                 imageSize,
                                                 note.imageResolution);

        noteTransform.scale = scale
        noteScaleInteraction.done()
    }

}
