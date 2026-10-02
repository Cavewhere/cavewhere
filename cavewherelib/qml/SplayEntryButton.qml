/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import cavewherelib

// The round "+" a station with no splays offers while the pointer is over its
// Splays cell — the way into typing a splay when the download died.
//
// The "+" is drawn from two bars rather than set from an SVG. An offscreen
// probe says plainly which half of that choice is forced: a plain Image of an
// SVG paints fine under `--platform offscreen`, while the same file through
// Icon paints nothing, because Icon takes its color from a MultiEffect layer.
// Taking the theme's splay color is the whole point here, so the bars draw it
// directly and the button renders the same offscreen as it does on screen.
//
// The Splays cell owns the tap: this is the mark, and the cell around it is the
// target.
QQ.Rectangle {
    id: entryButton

    readonly property real glyphLength: entryButton.width * Theme.splayEntryGlyphSpan

    implicitWidth: Theme.splayEntryButtonSize
    implicitHeight: Theme.splayEntryButtonSize

    width: Theme.splayEntryButtonSize
    height: Theme.splayEntryButtonSize
    radius: entryButton.width * 0.5

    color: Theme.splaySurface
    border.color: Theme.splayBorder
    border.width: 1

    QQ.Rectangle {
        anchors.centerIn: parent
        width: entryButton.glyphLength
        height: Theme.splayEntryGlyphThickness
        color: Theme.splayText
    }

    QQ.Rectangle {
        anchors.centerIn: parent
        width: Theme.splayEntryGlyphThickness
        height: entryButton.glyphLength
        color: Theme.splayText
    }
}
