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

QQ.Item {
    id: rootId
    objectName: "csCustomDialog"

    signal accepted(string cs)

    function open() {
        searchFieldId.text = ""
        searchModelId.query = ""
        listViewId.currentIndex = -1
        dialogId.open()
        searchFieldId.forceActiveFocus()
    }

    function close() {
        dialogId.close()
    }

    function commitDirectEntry() {
        const t = searchFieldId.text.trim()
        if (/^EPSG:\d+$/i.test(t) && CoordinateSystem.isValidCS(t)) {
            rootId.accepted(t.toUpperCase())
            dialogId.close()
        }
    }

    CRSSearchModel {
        id: searchModelId
    }

    QC.Dialog {
        id: dialogId

        anchors.centerIn: QC.Overlay.overlay
        modal: true
        implicitWidth: 480
        title: qsTr("Choose a coordinate system")
        closePolicy: QC.Popup.NoAutoClose
        standardButtons: QC.Dialog.Ok | QC.Dialog.Cancel

        onAccepted: {
            if (listViewId.currentIndex >= 0) {
                const idx = searchModelId.index(listViewId.currentIndex)
                const code = searchModelId.data(idx, CRSSearchModel.DisplayCodeRole)
                rootId.accepted(code)
            } else {
                rootId.commitDirectEntry()
            }
        }

        contentItem: ColumnLayout {
            spacing: 8

            QC.TextField {
                id: searchFieldId
                objectName: "csSearchField"
                Layout.fillWidth: true
                placeholderText: qsTr("Search by name or EPSG code")
                onTextChanged: searchModelId.query = text
                onAccepted: {
                    if (listViewId.currentIndex < 0) {
                        rootId.commitDirectEntry()
                    } else {
                        dialogId.accept()
                    }
                }
            }

            QC.Frame {
                Layout.fillWidth: true
                Layout.preferredHeight: 320
                padding: 0

                QQ.ListView {
                    id: listViewId
                    objectName: "csSearchList"
                    anchors.fill: parent
                    clip: true
                    model: searchModelId
                    currentIndex: -1

                    QC.ScrollBar.vertical: QC.ScrollBar {}

                    delegate: QC.ItemDelegate {
                        id: delegateId
                        required property int index
                        required property string displayCode
                        required property string name
                        required property bool isProjected

                        width: listViewId.width
                        highlighted: listViewId.currentIndex === delegateId.index
                        onClicked: listViewId.currentIndex = delegateId.index
                        onDoubleClicked: dialogId.accept()

                        contentItem: ColumnLayout {
                            spacing: 2
                            QC.Label {
                                Layout.fillWidth: true
                                text: delegateId.displayCode
                                font.bold: true
                                font.family: Theme.fontFamilyMono
                                elide: QC.Label.ElideRight
                            }
                            QC.Label {
                                Layout.fillWidth: true
                                text: delegateId.isProjected
                                      ? delegateId.name
                                      : qsTr("%1 (geographic)").arg(delegateId.name)
                                color: Theme.textSubtle
                                font.pixelSize: Theme.fontSizeSmall
                                elide: QC.Label.ElideRight
                            }
                        }
                    }
                }
            }

            QC.Label {
                Layout.fillWidth: true
                visible: listViewId.count === 0
                text: searchFieldId.text.length === 0
                      ? qsTr("Showing common projections. Type to search the full PROJ database.")
                      : qsTr("No matches. Type a different name or EPSG code, or paste a full \"EPSG:nnnn\" to use it directly.")
                color: Theme.textSubtle
                wrapMode: QC.Label.WordWrap
            }
        }
    }
}
