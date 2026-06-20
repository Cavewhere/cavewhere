/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// One row in the glyph library's left rail: the glyph's displayName with its
// dimmed machine name beneath. A rendered thumbnail replaces the placeholder in
// a later commit (§6). Selection is a TapHandler, the CaveWhere way.
QQ.Item {
    id: delegateId
    objectName: "glyphListDelegate"

    // Bound from the model roles by the ListView.
    required property int index
    required property string name
    required property string displayName
    required property int strokeCount

    property bool highlighted: false

    signal clicked()

    implicitHeight: rowId.implicitHeight + Theme.tightSpacing * 2

    QQ.Rectangle {
        anchors.fill: parent
        color: delegateId.highlighted ? Theme.highlight : Theme.transparent
    }

    RowLayout {
        id: rowId
        anchors.fill: parent
        anchors.leftMargin: Theme.pageMargin
        anchors.rightMargin: Theme.pageMargin
        spacing: Theme.tightSpacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            QC.Label {
                Layout.fillWidth: true
                text: delegateId.displayName.length > 0 ? delegateId.displayName : delegateId.name
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                elide: QC.Label.ElideRight
            }

            QC.Label {
                Layout.fillWidth: true
                text: delegateId.name
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeCaption
                color: Theme.textSubtle
                elide: QC.Label.ElideRight
            }
        }

        // Empty-glyph hint, kept subtle (the model's strokeCount role).
        QC.Label {
            visible: delegateId.strokeCount === 0
            text: "empty"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeCaption
            color: Theme.textSubtle
        }
    }

    QQ.TapHandler {
        onTapped: delegateId.clicked()
    }
}
