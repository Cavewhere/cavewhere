import QtQuick 2.0
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.15 as QC
import QtQml.Models 2.15
import cavewherelib 1.0

Item {
    id: keywordTabId
    objectName: "keyword"

    readonly property KeywordFilterPipelineModel pipelineModel: RootData.keywordFilterPipelineModel

    KeywordFilterOrGroupProxyModel {
        id: orProxyModelId
        sourceModel: pipelineModel
    }

    KeywordVisibility {
        visibleModel: pipelineModel.acceptedModel
        hideModel: pipelineModel.rejectedModel
    }

    ColumnLayout {
        anchors.fill: parent
        // spacing:

        ListView {
            id: groupListView
            objectName: "groupListView"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 3
            highlightFollowsCurrentItem: true
            model: orProxyModelId

            QC.ScrollBar.vertical: QC.ScrollBar {
                policy: QC.ScrollBar.AsNeeded
                objectName: "vScrollBar"
            }

            delegate:  Rectangle {
                id: orDelegateId
                // required property int lastSourceRow
                required property int index

                width: ListView.view ? ListView.view.width : parent.width
                height: Math.max(400, groupListView.height / groupListView.count)

                border {
                    color: "#ababab"
                    width: 1
                }

                ListView {
                    id: andListView
                    objectName: "andListView_" + orDelegateId.index
                    orientation: ListView.Horizontal
                    spacing: 12
                    clip: true
                    highlightFollowsCurrentItem: true

                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        bottom: parent.bottom
                        margins: 5
                    }

                    Layout.fillWidth: true

                    QC.ScrollBar.horizontal: QC.ScrollBar {
                        id: groupScrollBarId
                        objectName: "groupScrollBar"
                        policy: QC.ScrollBar.AsNeeded

                        onPositionChanged: {
                            console.log("Position changed:" + position)
                        }
                    }

                    model: KeywordFilterGroupProxyModel {
                        id: groupProxyModel
                        sourceModel: pipelineModel
                        groupIndex: orDelegateId.index
                    }

                    delegate: Item {
                        id: delegateId

                        objectName: "delegate_" + delegateId.index

                        required property int sourceRow
                        required property KeywordGroupByKeyModel filterModelObjectRole
                        required property int index

                        width: 220
                        height: andListView.height

                        Rectangle {
                            visible: delegateId.index % 2 === 0
                            color: "#efefef"
                            anchors.fill: columnLayoutId
                        }

                        ColumnLayout {
                            id: columnLayoutId

                            spacing: 4

                            anchors.fill: parent

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                QC.ComboBox {
                                    id: keyCombo
                                    objectName: "keyCombo"
                                    Layout.fillWidth: true
                                    model: pipelineModel.possibleKeys
                                    currentIndex: filterModelObjectRole.key && model.indexOf(filterModelObjectRole.key) >= 0
                                                  ? model.indexOf(filterModelObjectRole.key)
                                                  : 0
                                    Component.onCompleted: {
                                        if(!filterModelObjectRole.key && model.length > 0) {
                                            filterModelObjectRole.key = model[0]
                                        }
                                    }
                                    onActivated: filterModelObjectRole.key = currentText
                                }

                                NoteToolIconButton {
                                    id: removeButton
                                    objectName: "removeButton"
                                    visible: pipelineModel.rowCount() > 1
                                    iconSource: "qrc:/twbs-icons/icons/dash.svg"
                                    onClicked: pipelineModel.removeRow(sourceRow)
                                }

                                NoteToolIconButton {
                                    id: addButton
                                    objectName: "addButton"
                                    icon.source: "qrc:/twbs-icons/icons/plus.svg"
                                    onClicked: {
                                        pipelineModel.insertRow(sourceRow + 1)
                                        andListView.currentIndex = delegateId.index + 1
                                    }
                                }
                            }

                            ListView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                // implicitHeight: contentItem.childrenRect.height
                                model: filterModelObjectRole
                                clip: true
                                spacing: 4

                                QC.ScrollBar.vertical: QC.ScrollBar {
                                    policy: QC.ScrollBar.AsNeeded
                                }

                                delegate: RowLayout {
                                    id: rowId
                                    objectName: "row" + rowId.index;

                                    required property bool acceptedRole
                                    required property string valueRole
                                    required property int objectCountRole
                                    required property int index
                                    spacing: 6

                                    QC.CheckBox {
                                        objectName: "checkbox"
                                        checked: acceptedRole
                                        onToggled: {
                                            var modelIndex = filterModelObjectRole.index(index, 0);
                                            filterModelObjectRole.setData(modelIndex, checked, KeywordGroupByKeyModel.AcceptedRole);
                                        }
                                    }

                                    Text {
                                        text: valueRole + " (" + objectCountRole + ")"
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        QC.Button {
            objectName: "alsoButton"
            text: qsTr("Also Include")
            Layout.fillWidth: true
            onClicked: {
                pipelineModel.addRow();
                let last = pipelineModel.rowCount() - 1;
                let idx = pipelineModel.index(last, 0);
                pipelineModel.setData(idx, KeywordFilterPipelineModel.Or, KeywordFilterPipelineModel.OperatorRole);
                groupListView.currentIndex = groupListView.count - 1
            }
        }
    }
}
