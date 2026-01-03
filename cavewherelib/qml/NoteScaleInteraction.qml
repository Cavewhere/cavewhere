/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import cavewherelib

DrawLengthInteraction {
    id: noteScaleInteraction
    objectName: "noteScaleInteraction"

    property NoteTranformation noteTransform
    required property Note note
    required property ScrapView scrapView

    doneTextLabel: "<b>In cave length</b>"

    onDoneButtonPressed: {
        let p1 = scrapView.toNoteCoordinates(firstMouseLocation);
        let p2 = scrapView.toNoteCoordinates(secondMouseLocation);
        let scale = noteTransform.calculateScaleForRendered(p1,
                                                            p2,
                                                            lengthObject,
                                                            note.image,
                                                            note.renderSize(),
                                                            note.imageResolution);

        noteTransform.scale = scale
        noteScaleInteraction.done()
    }

}
