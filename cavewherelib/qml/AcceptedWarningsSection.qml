pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// The trip's accepted warnings, under the warning badge. One line while
// collapsed; open, it groups the warnings by kind, rare kinds first, so a big
// backsight disagreement accepted along with hundreds of missing LRUDs is easy
// to find. Unticking a row un-accepts it and tapping its text focuses its
// cell. Lives in the survey table's header, so it holds no Flickable and only
// builds rows for the groups that are open.
ColumnLayout {
    id: sectionId
    objectName: "acceptedWarningsSection"

    required property Trip trip
    required property SurveyEditorModel surveyEditorModel

    property bool expanded: false
    property list<string> expandedKinds: []

    readonly property int acceptedCount: trip === null ? 0 : trip.errorModel.suppressedWarningCount

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

    visible: acceptedCount > 0

    onTripChanged: {
        sectionId.expanded = false
        sectionId.expandedKinds = []
    }

    ErrorTreeModel {
        id: acceptedModelId
        root: sectionId.trip === null ? null : sectionId.trip.errorModel
        suppressedOnly: true
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
                    objectName: "acceptedWarningsHeader"
                    Layout.fillWidth: true

                    QQ.TapHandler {
                        onTapped: sectionId.expanded = !sectionId.expanded
                    }

                    Chevron {
                        open: sectionId.expanded
                    }

                    QC.Label {
                        objectName: "acceptedWarningsHeaderText"
                        Layout.fillWidth: true
                        text: sectionId.acceptedCount === 1 ? qsTr("1 accepted warning")
                                                            : qsTr("%1 accepted warnings").arg(sectionId.acceptedCount)
                    }
                }

                LinkText {
                    objectName: "unacceptAllAccepted"
                    text: qsTr("Un-accept all")
                    onClicked: sectionId.trip.clearAcceptedWarnings()
                }
            }

            QQ.Loader {
                Layout.fillWidth: true
                active: sectionId.expanded
                visible: active

                sourceComponent: ColumnLayout {
                    // Every kind keeps its group, hidden while empty, so un-accepting
                    // one group leaves the others and their open state alone
                    QQ.Repeater {
                        model: acceptedModelId.allKinds

                        // One kind of warning: a collapsible header with the
                        // count and a link that un-accepts the whole group,
                        // then its rows once opened
                        delegate: ColumnLayout {
                            id: groupId
                            objectName: "acceptedWarningsGroup." + displayIndex

                            required property string modelData

                            readonly property string kind: modelData
                            readonly property int displayIndex: acceptedModelId.kinds.indexOf(kind)
                            readonly property int count: groupModelId.count
                            readonly property bool expanded: sectionId.expandedKinds.indexOf(kind) >= 0
                            readonly property list<int> typeIds: groupModelId.typeIdsForKind(kind)

                            Layout.fillWidth: true
                            visible: displayIndex >= 0

                            ErrorTreeModel {
                                id: groupModelId
                                root: acceptedModelId.root
                                suppressedOnly: true
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
                                    objectName: "unacceptGroup"
                                    visible: groupId.typeIds.length > 0
                                    text: qsTr("Un-accept %1").arg(groupId.count)
                                    onClicked: sectionId.trip.clearAcceptedWarnings(groupId.typeIds)
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
                                            objectName: "acceptedWarningRow." + index

                                            required property int index
                                            required property ErrorModel errorModel

                                            Layout.fillWidth: true

                                            onAcceptToggled: (accepted) => {
                                                groupModelId.setData(groupModelId.index(rowId.index, 0),
                                                                     accepted,
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
