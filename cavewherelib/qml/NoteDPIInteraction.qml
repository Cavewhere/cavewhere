/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib

DrawLengthInteraction {
    id: noteDPIInteraction

    required property Note note
    required property ImageResolution imageResolution

    doneTextLabel: "<b>Length on the paper</b>"
    defaultLengthUnit: Units.Inches
    doneEnabled: lengthObject.value > 0.0
    lengthValidator: DistanceValidator {}

    onDoneButtonPressed: {
        var p1 = firstMouseLocation;
        var p2 = secondMouseLocation;

        var xImage = (p2.x - p1.x);
        var yImage = (p2.y - p1.y);

        var lengthPixels = Math.sqrt(xImage * xImage + yImage * yImage)
        var nativePixels = lengthPixels
        if(note !== null && imageItem !== null && imageResolution !== null) {
            nativePixels = imageResolution.nativePixelLength(note.image,
                                                            imageItem.sourceSize,
                                                            lengthPixels)
        }

        imageResolution.setResolution(lengthObject, nativePixels)

        noteDPIInteraction.done()
    }
}
