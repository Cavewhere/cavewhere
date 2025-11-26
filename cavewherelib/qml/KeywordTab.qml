import QtQuick 2.0
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.15 as QC
import QtQml.Models 2.15
import cavewherelib 1.0

Item {
    id: keywordTabId

    KeywordFilterPipelineModel {
        id: pipelineModelId
        keywordModel: RootData.keywordItemModel
    }

    KeywordFilterOrGroupProxyModel {
        id: orProxyModelId
        sourceModel: pipelineModelId
    }

    KeywordVisibility {
        visibleModel: pipelineModelId.acceptedModel
        hideModel: pipelineModelId.rejectedModel
    }

    ColumnLayout {
        anchors.fill: parent
        // spacing:

        ListView {
            id: groupListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: orProxyModelId

            QC.ScrollBar.vertical: QC.ScrollBar {
                policy: QC.ScrollBar.AsNeeded
            }

            delegate:  Rectangle {
                id: orDelegateId
                required property int groupIndex
                required property int firstSourceRow
                required property int lastSourceRow
                required property int index

                width: ListView.view ? ListView.view.width : parent.width
                height: Math.max(400, groupListView.height / groupListView.count)

                border {
                    color: "#ababab"
                    width: 1
                }

                ListView {
                    id: andListView
                    property int groupLastSourceRow: lastSourceRow

                    orientation: ListView.Horizontal
                    spacing: 12
                    clip: true

                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        bottom: parent.bottom
                        margins: 5
                    }

                    Layout.fillWidth: true

                    QC.ScrollBar.horizontal: QC.ScrollBar {
                        policy: QC.ScrollBar.AsNeeded
                    }

                    model: DelegateModel {
                        id: groupDelegateModel
                        model: orProxyModelId
                        rootIndex: orProxyModelId.groupModelIndex(groupIndex)
                        delegate: Item {
                            id: delegateId

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
                                        Layout.fillWidth: true
                                        model: pipelineModelId.possibleKeys
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
                                        objectName: "removeButton" + orDelegateId.index + "_" + delegateId.index
                                        visible: pipelineModelId.rowCount() > 1
                                        iconSource: "qrc:/twbs-icons/icons/dash.svg"
                                        onClicked: pipelineModelId.removeRow(sourceRow)
                                    }

                                    NoteToolIconButton {
                                        id: addButton
                                        objectName: "addButton" + orDelegateId.index + "_" + delegateId.index
                                        icon.source: "qrc:/twbs-icons/icons/plus.svg"
                                        onClicked: pipelineModelId.insertRow(andListView.groupLastSourceRow + 1)
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
                                        required property bool acceptedRole
                                        required property string valueRole
                                        required property int objectCountRole
                                        required property int index
                                        spacing: 6

                                        QC.CheckBox {
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
        }

        QC.Button {
            text: qsTr("Also Include")
            Layout.fillWidth: true
            onClicked: {
                pipelineModelId.addRow();
                var last = pipelineModelId.rowCount() - 1;
                var idx = pipelineModelId.index(last, 0);
                pipelineModelId.setData(idx, KeywordFilterPipelineModel.Or, KeywordFilterPipelineModel.OperatorRole);
            }
        }
    }
}
