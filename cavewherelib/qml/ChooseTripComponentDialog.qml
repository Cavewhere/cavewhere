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

// Multi-component picker for the sketch line plot. The sketch's parent trip
// has more than one connected survey component and the sketch has no stored
// anchor (first open) — the caver picks which component to sketch over.
//
// This dialog is intended to be instantiated inside a Loader gated by the
// parent page's Component.onCompleted, so it cannot appear during transient
// property thrash or before the page is attached to the scene.
QQ.Item {
    id: rootId

    property Sketch sketch: null
    property var components: []

    signal anchorChosen(string anchor)

    function open(summaries) {
        components = summaries
        if (summaries.length > 0) {
            componentListId.currentIndex = 0
        }
        dialogId.open()
    }

    function close() {
        dialogId.close()
    }

    QC.Dialog {
        id: dialogId
        objectName: "chooseTripComponentDialog"

        anchors.centerIn: QC.Overlay.overlay
        modal: true
        implicitWidth: 420
        title: qsTr("Pick a survey component")
        closePolicy: QC.Popup.NoAutoClose

        onAccepted: {
            if (componentListId.currentIndex >= 0
                && componentListId.currentIndex < rootId.components.length) {
                const picked = rootId.components[componentListId.currentIndex]
                if (rootId.sketch) {
                    rootId.sketch.anchorStation = picked.anchor
                }
                rootId.anchorChosen(picked.anchor)
            }
        }

        contentItem: ColumnLayout {
            spacing: 8

            BodyText {
                Layout.fillWidth: true
                wrapMode: QC.Label.WordWrap
                text: qsTr("This trip's survey has more than one connected component. "
                         + "Pick the one to draw over — the sketch will anchor to that "
                         + "component's first station.")
            }

            QC.Frame {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                padding: 0

                QQ.ListView {
                    id: componentListId
                    objectName: "componentList"
                    anchors.fill: parent
                    clip: true
                    model: rootId.components

                    delegate: QC.ItemDelegate {
                        required property int index
                        required property var modelData
                        width: componentListId.width
                        highlighted: componentListId.currentIndex === index
                        onClicked: componentListId.currentIndex = index

                        contentItem: ColumnLayout {
                            spacing: 2
                            QC.Label {
                                Layout.fillWidth: true
                                text: qsTr("Anchor: %1").arg(parent.parent.modelData.anchor)
                                font.bold: true
                                elide: QC.Label.ElideRight
                            }
                            QC.Label {
                                Layout.fillWidth: true
                                text: qsTr("%1 stations").arg(parent.parent.modelData.stationCount)
                                color: Theme.textSubtle
                                font.pixelSize: Theme.fontSizeSmall
                            }
                        }
                    }
                }
            }
        }

        standardButtons: QC.Dialog.Ok | QC.Dialog.Cancel
    }
}
