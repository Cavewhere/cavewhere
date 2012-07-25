import QtQuick 2.0
import Cavewhere 1.0

DrawLengthInteraction {
    id: noteDPIInteraction

    property ImageResolution imageResolution

    doneTextLabel: "<b>Length on the paper</b>"
    defaultLengthUnit: Units.Inches

    onDoneButtonPressed: {

        var p1 = firstMouseLocation;
        var p2 = secondMouseLocation;
        var imageSize = imageItem.imageProperties.size

        var xImage = (p2.x - p1.x) * imageSize.width
        var yImage = (p2.y - p1.y) * imageSize.height

        var lengthPixels = Math.sqrt(xImage * xImage + yImage * yImage)

        console.log("Length pixels:" + lengthPixels)

        imageResolution.setResolution(lengthObject, lengthPixels)

        noteScaleInteraction.done()
    }

}
