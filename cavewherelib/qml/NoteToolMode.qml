/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ

pragma Singleton

// The tool vocabulary shared by NotesGallery and the note editors it drives
// (NoteItem, NoteLiDARItem). These name QQ.State objects, so they have to stay
// strings — this singleton just keeps the spelling in one place instead of
// re-declared across four files.
//
// The *Mode values are the coarse grouping NotesGallery.mode reports: every
// state other than none/noNotes is a carpet-editing state.
QQ.QtObject {
    readonly property string none: ""
    readonly property string noNotes: "NO_NOTES"
    readonly property string carpet: "CARPET"
    readonly property string select: "SELECT"
    readonly property string addStation: "ADD-STATION"
    readonly property string addScrap: "ADD-SCRAP"
    readonly property string addLead: "ADD-LEAD"
    readonly property string addSketchWall: "ADD-SKETCH-WALL"
    readonly property string addSketchFeature: "ADD-SKETCH-FEATURE"
    readonly property string captureIcon: "CAPTURE_ICON"

    //carpetMode and carpet share a spelling but not a vocabulary: compare
    //*Mode values against NotesGallery.mode, and the rest against a state.
    //Swapping them compares equal, so no test or lint can catch it.
    readonly property string defaultMode: "DEFAULT"
    readonly property string carpetMode: "CARPET"
}
