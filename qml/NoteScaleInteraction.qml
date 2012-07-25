import QtQuick 2.0
import Cavewhere 1.0

DrawLengthInteraction {
    id: noteScaleInteraction

    property NoteTransform noteTransform

    doneTextLabel: "<b>In cave length</b>"

    onDoneButtonPressed: {
        var imageSize = imageItem.imageProperties.size
        var dotPerMeter = imageItem.imageProperties.dotsPerMeter
        var scale = noteTransform.calculateScale(firstMouseLocation,
                                                 secondMouseLocation,
                                                 lengthObject,
                                                 imageSize,
                                                 dotPerMeter);

        noteTransform.scale = scale
        noteScaleInteraction.done()
    }

}
