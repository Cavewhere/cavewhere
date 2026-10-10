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

// The source card on a derived node's page: which file the node comes from,
// what the node is inside that file, and a link to the source root, whose page
// holds Reload, Replace…, and Restore….
QQ.Item {
    id: root
    objectName: "derivedNodeCard"

    property SurveyNode node: null
    property NodePageModel pageModel: null

    // Read once per node: a derived node stays under the root that built it.
    readonly property SurveyNode sourceRoot: root.node !== null ? root.node.sourceRoot() : null

    readonly property string entryFile: root.sourceRoot !== null
                                        ? root.sourceRoot.externalCenterline.entryFile : ""
    readonly property string fileName: root.entryFile.substring(root.entryFile.lastIndexOf("/") + 1)

    implicitWidth: contentColumnId.implicitWidth
    implicitHeight: contentColumnId.implicitHeight

    ColumnLayout {
        id: contentColumnId

        anchors.fill: parent
        spacing: Theme.tightSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.tightSpacing

            QC.Label {
                objectName: "derivedNodeFileLabel"
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

            QQ.Item { Layout.fillWidth: true }

            LinkText {
                objectName: "derivedNodeOpenRootLink"
                visible: root.sourceRoot !== null
                text: root.sourceRoot !== null ? qsTr("Open %1").arg(root.sourceRoot.name) : ""
                onClicked: RootData.pageSelectionModel.currentPageAddress =
                           linkGeneratorId.nodeLink(root.sourceRoot)
            }
        }

        QC.Label {
            objectName: "derivedNodeUnitLabel"
            Layout.fillWidth: true
            wrapMode: QC.Label.WordWrap
            color: Theme.textSecondary
            text: root.node !== null ? qsTr("This node is %1 in the project").arg(root.node.sourcePath) : ""
        }

        QC.Label {
            objectName: "derivedNodeCountsLabel"
            Layout.fillWidth: true
            color: Theme.textSecondary
            text: root.pageModel !== null
                  ? qsTr("%n unit(s)", "", root.pageModel.sourceUnitCount)
                    + " · "
                    + qsTr("%n station(s)", "", root.pageModel.stationCount)
                  : ""
        }
    }

    LinkGenerator {
        id: linkGeneratorId
        pageSelectionModel: RootData.pageSelectionModel
    }
}
