/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib

DrawLengthInteraction {
    id: noteScaleInteraction

    property NoteTranformation noteTransform
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
