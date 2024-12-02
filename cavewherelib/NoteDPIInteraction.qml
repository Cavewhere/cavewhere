/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib

DrawLengthInteraction {
    id: noteDPIInteraction

    property ImageResolution imageResolution

    doneTextLabel: "<b>Length on the paper</b>"
    defaultLengthUnit: Units.Inches

    onDoneButtonPressed: {

        var p1 = firstMouseLocation;
        var p2 = secondMouseLocation;
        var imageSize = imageItem.sourceSize

        var xImage = (p2.x - p1.x) * imageSize.width
        var yImage = (p2.y - p1.y) * imageSize.height

        var lengthPixels = Math.sqrt(xImage * xImage + yImage * yImage)

        imageResolution.setResolution(lengthObject, lengthPixels)

        noteDPIInteraction.done()
    }

}
