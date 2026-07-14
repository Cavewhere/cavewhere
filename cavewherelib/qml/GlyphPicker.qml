/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import cavewherelib

// Glyph picker for a brush layer: shows the current glyph's display name (or
// "Line layer" when none) and opens a popup list of the palette's glyphs plus a
// "Line layer (no glyph)" clear row. Tapping a row emits glyphSelected with the
// glyph's machine name, or an empty string to clear the layer back to a line
// (offset-polyline) layer. cwSymbologyGlyph is not a QML gadget, so the list is
// driven by GlyphListModel rather than enumerating palette.glyphs() directly.
QC.Control {
    id: rootId

    property SymbologyPalette palette
    property string currentGlyphName

    readonly property int kBorderWidth: 1
    readonly property int kPopupMinWidth: 180
    readonly property int kPopupMaxHeight: 320

    // Empty machine name clears the glyph (line layer); any other name selects
    // that glyph. The consumer writes the layer's glyph in response.
    signal glyphSelected(string name)

    // The human label for a glyph machine name, falling back to the raw name
    // when the palette doesn't resolve it (e.g. a dangling reference).
    function displayNameFor(name) {
        if (name === "") {
            return "Line layer"
        }
        const shown = rootId.palette === null ? "" : rootId.palette.glyphDisplayName(name)
        return shown === "" ? name : shown
    }

    padding: 0

    contentItem: QQ.Row {
        spacing: Theme.tightSpacing

        QC.Label {
            objectName: "layerGlyphName"
            text: rootId.displayNameFor(rootId.currentGlyphName)
            color: Theme.textSubtle
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeCaption
            verticalAlignment: QQ.Text.AlignVCenter
        }

        QC.Label {
            text: "▾"
            color: Theme.textSubtle
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeCaption
            verticalAlignment: QQ.Text.AlignVCenter
        }
    }

    QQ.TapHandler {
        onTapped: popupId.opened ? popupId.close() : popupId.open()
    }

    GlyphListModel {
        id: glyphModelId
        palette: rootId.palette
    }

    QC.Popup {
        id: popupId
        objectName: "layerGlyphPickerPopup"

        y: rootId.height
        width: Math.max(rootId.width, rootId.kPopupMinWidth)
        implicitHeight: Math.min(listId.contentHeight + topPadding + bottomPadding, rootId.kPopupMaxHeight)
        padding: rootId.kBorderWidth

        background: QQ.Rectangle {
            color: Theme.surface
            border.color: Theme.border
            border.width: rootId.kBorderWidth
            radius: Theme.floatingWidgetRadius
        }

        contentItem: QC.ScrollView {
            QQ.ListView {
                id: listId
                model: glyphModelId
                clip: true
                boundsBehavior: QQ.Flickable.StopAtBounds

                // "Line layer (no glyph)" clear option, pinned to the top with
                // OverlayHeader so it stays reachable without scrolling even when
                // the glyph list overflows the popup.
                headerPositioning: QQ.ListView.OverlayHeader
                header: QQ.Rectangle {
                    objectName: "layerGlyphClearRow"
                    width: listId.width
                    implicitHeight: clearLabelId.implicitHeight
                    z: 1
                    // Opaque (not transparent) so the pinned overlay header occludes
                    // glyph rows scrolling beneath it.
                    color: rootId.currentGlyphName === "" ? Theme.highlight : Theme.surface

                    QC.Label {
                        id: clearLabelId
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Line layer (no glyph)"
                        elide: QQ.Text.ElideRight
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        leftPadding: Theme.delegatePadding
                        rightPadding: Theme.delegatePadding
                        topPadding: Theme.tightSpacing
                        bottomPadding: Theme.tightSpacing
                    }

                    QQ.TapHandler {
                        onTapped: {
                            rootId.glyphSelected("")
                            popupId.close()
                        }
                    }
                }

                delegate: QQ.Rectangle {
                    id: glyphDelegateId

                    required property int index
                    required property string name
                    required property string displayName

                    readonly property bool isSelected: glyphDelegateId.name === rootId.currentGlyphName

                    objectName: "layerGlyphRow_" + glyphDelegateId.name
                    width: listId.width
                    implicitHeight: glyphLabelId.implicitHeight
                    color: glyphDelegateId.isSelected ? Theme.highlight : Theme.transparent

                    QC.Label {
                        id: glyphLabelId
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: glyphDelegateId.displayName
                        elide: QQ.Text.ElideRight
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        leftPadding: Theme.delegatePadding
                        rightPadding: Theme.delegatePadding
                        topPadding: Theme.tightSpacing
                        bottomPadding: Theme.tightSpacing
                    }

                    QQ.TapHandler {
                        onTapped: {
                            rootId.glyphSelected(glyphDelegateId.name)
                            popupId.close()
                        }
                    }
                }
            }
        }
    }
}
