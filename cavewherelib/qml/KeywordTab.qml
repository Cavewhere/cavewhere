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
        spacing: 12

        ListView {
            id: groupListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 12
            model: orProxyModelId

            delegate: ColumnLayout {
                required property int groupIndex
                required property int firstSourceRow
                required property int lastSourceRow

                spacing: 6
                width: ListView.view ? ListView.view.width : parent.width

                RowLayout {
                    Text {
                        text: qsTr("Match all of these")
                        font.bold: true
                    }

                    QC.Button {
                        text: qsTr("Add")
                        onClicked: pipelineModelId.insertRow(lastSourceRow + 1)
                    }
                }

                ListView {
                    id: andListView
                    orientation: ListView.Horizontal
                    spacing: 12
                    clip: true
                    Layout.fillWidth: true
                    height: 400
                    model: DelegateModel {
                        model: orProxyModelId
                        rootIndex: {
                            console.log("RootIndex:" + orProxyModelId.groupModelIndex(groupIndex))
                            return orProxyModelId.groupModelIndex(groupIndex)
                        }
                        delegate: ColumnLayout {
                            required property int sourceRow
                            required property KeywordGroupByKeyModel filterModelObjectRole

                            spacing: 4
                            width: 220

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

                            ListView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                implicitHeight: contentItem.childrenRect.height
                                model: filterModelObjectRole
                                clip: true
                                spacing: 4

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

        RowLayout {
            Layout.alignment: Qt.AlignLeft
            spacing: 8

            QC.Button {
                text: qsTr("Also Include")
                onClicked: {
                    pipelineModelId.addRow();
                    var last = pipelineModelId.rowCount() - 1;
                    var idx = pipelineModelId.index(last, 0);
                    pipelineModelId.setData(idx, KeywordFilterPipelineModel.Or, KeywordFilterPipelineModel.OperatorRole);
                }
            }
        }
    }
}
