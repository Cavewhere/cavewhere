/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// The source card on a derived node's page: the file the node comes from and
// that file's format.
QQ.Item {
    id: root
    objectName: "derivedNodeCard"

    property SurveyNode node: null

    // Read once per node: a derived node stays under the root that built it.
    readonly property SurveyNode sourceRoot: root.node !== null ? root.node.sourceRoot() : null

    readonly property string entryFile: root.sourceRoot !== null
                                        ? root.sourceRoot.externalCenterline.entryFile : ""
    readonly property string fileName: root.entryFile.substring(root.entryFile.lastIndexOf("/") + 1)

    implicitWidth: contentRowId.implicitWidth
    implicitHeight: contentRowId.implicitHeight

    RowLayout {
        id: contentRowId

        anchors.fill: parent
        spacing: Theme.tightSpacing

        QC.Label {
            objectName: "derivedNodeFileLabel"
            Layout.fillWidth: true
            Layout.maximumWidth: implicitWidth
            elide: QC.Label.ElideMiddle
            font.bold: true
            text: root.fileName
        }

        QC.Label {
            objectName: "derivedNodeFormatLabel"
            visible: text.length > 0
            color: Theme.textSecondary
            text: root.sourceRoot !== null ? root.sourceRoot.externalCenterline.format : ""
        }
    }
}
