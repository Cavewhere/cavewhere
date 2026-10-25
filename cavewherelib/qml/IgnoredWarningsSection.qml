pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// The trip's ignored warnings, under the warning badge. One line while
// collapsed; open, it groups the warnings by kind, rare kinds first, so a big
// backsight disagreement ignored along with hundreds of missing LRUDs is easy
// to find. Unticking a row shows that warning again and tapping its text focuses its
// cell. Lives in the survey table's header, so it holds no Flickable and only
// builds rows for the groups that are open.
ColumnLayout {
    id: sectionId
    objectName: "ignoredWarningsSection"

    required property Trip trip
    required property SurveyEditorModel surveyEditorModel

    property bool expanded: false
    property list<string> expandedKinds: []

    readonly property int ignoredCount: trip === null ? 0 : trip.errorModel.ignoredWarningCount

    function setKindExpanded(kind: string, open: bool) {
        const others = sectionId.expandedKinds.filter(existing => existing !== kind)
        sectionId.expandedKinds = open ? others.concat([kind]) : others
    }

    function focusCellOf(errorModel: ErrorModel) {
        for (let i = 0; i < sectionId.trip.chunkCount; i++) {
            const chunk = sectionId.trip.chunk(i)
            const cell = chunk.cellIndexForErrorModel(errorModel)
            if (cell.valid) {
                const row = sectionId.surveyEditorModel.modelRowForCellRole(chunk, cell.index, cell.role)
                if (row >= 0) {
                    sectionId.surveyEditorModel.setFocusedCell(sectionId.surveyEditorModel.cellIndex(row, cell.role))
                }
                return
            }
        }
    }

    visible: ignoredCount > 0

    onTripChanged: {
        sectionId.expanded = false
        sectionId.expandedKinds = []
    }

    ErrorTreeModel {
        id: ignoredModelId
        root: sectionId.trip === null ? null : sectionId.trip.errorModel
        ignoredOnly: true
    }

    QQ.Rectangle {
        Layout.fillWidth: true
        implicitHeight: contentId.implicitHeight + 2 * Theme.statsPadding

        color: Theme.sectionFill
        radius: Theme.panelRadius

        ColumnLayout {
            id: contentId

            anchors.fill: parent
            anchors.margins: Theme.statsPadding

            RowLayout {
                Layout.fillWidth: true

                RowLayout {
                    objectName: "ignoredWarningsHeader"
                    Layout.fillWidth: true

                    QQ.TapHandler {
                        onTapped: sectionId.expanded = !sectionId.expanded
                    }

                    Chevron {
                        open: sectionId.expanded
                    }

                    QC.Label {
                        objectName: "ignoredWarningsHeaderText"
                        Layout.fillWidth: true
                        text: sectionId.ignoredCount === 1 ? qsTr("1 ignored warning")
                                                            : qsTr("%1 ignored warnings").arg(sectionId.ignoredCount)
                    }
                }

                LinkText {
                    objectName: "showAllIgnored"
                    text: qsTr("Show all")
                    onClicked: sectionId.trip.clearIgnoredWarnings()
                }
            }

            QQ.Loader {
                Layout.fillWidth: true
                active: sectionId.expanded
                visible: active

                sourceComponent: ColumnLayout {
                    // Every kind keeps its group, hidden while empty, so showing
                    // one group leaves the others and their open state alone
                    QQ.Repeater {
                        model: ignoredModelId.allKinds

                        // One kind of warning: a collapsible header with the
                        // count and a link that shows the whole group,
                        // then its rows once opened
                        delegate: ColumnLayout {
                            id: groupId
                            objectName: "ignoredWarningsGroup." + displayIndex

                            required property string modelData

                            readonly property string kind: modelData
                            readonly property int displayIndex: ignoredModelId.kinds.indexOf(kind)
                            readonly property int count: groupModelId.count
                            readonly property bool expanded: sectionId.expandedKinds.indexOf(kind) >= 0
                            readonly property list<int> typeIds: groupModelId.typeIdsForKind(kind)

                            Layout.fillWidth: true
                            visible: displayIndex >= 0

                            ErrorTreeModel {
                                id: groupModelId
                                root: ignoredModelId.root
                                ignoredOnly: true
                                kindFilter: groupId.kind
                            }

                            RowLayout {
                                Layout.fillWidth: true

                                RowLayout {
                                    objectName: "groupHeader"
                                    Layout.fillWidth: true

                                    QQ.TapHandler {
                                        onTapped: sectionId.setKindExpanded(groupId.kind, !groupId.expanded)
                                    }

                                    Chevron {
                                        open: groupId.expanded
                                    }

                                    QC.Label {
                                        Layout.fillWidth: true
                                        text: qsTr("%1 (%2)").arg(groupId.kind).arg(groupId.count)
                                    }
                                }

                                LinkText {
                                    objectName: "showGroup"
                                    visible: groupId.typeIds.length > 0
                                    text: qsTr("Show %1").arg(groupId.count)
                                    onClicked: sectionId.trip.clearIgnoredWarnings(groupId.typeIds)
                                }
                            }

                            QQ.Loader {
                                Layout.fillWidth: true
                                Layout.leftMargin: Theme.statsPadding
                                active: groupId.expanded
                                visible: active

                                sourceComponent: ColumnLayout {
                                    QQ.Repeater {
                                        model: groupModelId

                                        delegate: ErrorRow {
                                            id: rowId
                                            objectName: "ignoredWarningRow." + index

                                            required property int index
                                            required property ErrorModel errorModel

                                            Layout.fillWidth: true

                                            onIgnoreToggled: (ignored) => {
                                                groupModelId.setData(groupModelId.index(rowId.index, 0),
                                                                     ignored,
                                                                     ErrorListModel.SuppressedRole)
                                            }
                                            onMessageTapped: sectionId.focusCellOf(rowId.errorModel)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    component Chevron: QQ.Item {
        id: chevronId

        required property bool open

        implicitWidth: Theme.iconSizeButton
        implicitHeight: Theme.iconSizeButton

        Icon {
            anchors.centerIn: parent
            source: chevronId.open ? "qrc:/twbs-icons/icons/chevron-down.svg"
                                   : "qrc:/twbs-icons/icons/chevron-right.svg"
            sourceSize: Qt.size(Theme.chevronSize, Theme.chevronSize)
        }
    }
}
