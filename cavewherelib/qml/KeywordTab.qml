import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

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
                color: Theme.background

                border {
                    color: Theme.border
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
                            color: Theme.surfaceMuted
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

                            SelectionProxyModel {
                                id: keywordSelection
                                sourceModel: filterModelObjectRole
                                idRole: KeywordGroupByKeyModel.ValueRole
                            }

                            ListView {
                                id: keywordList
                                objectName: "keywordList"
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                model: keywordSelection
                                clip: true
                                // spacing: 4
                                property int anchorIndex: -1

                                QC.ScrollBar.vertical: QC.ScrollBar {
                                    policy: QC.ScrollBar.AsNeeded
                                }

                                function applyCheckedToSelection(value, targetIndex) {
                                    let selectionApplies = keywordSelection.selectionCount > 1 && keywordSelection.data(keywordSelection.index(targetIndex, 0), keywordSelection.selectionRole);
                                    if (!selectionApplies) {
                                        let idx = keywordSelection.index(targetIndex, 0);
                                        keywordSelection.setData(idx, value, KeywordGroupByKeyModel.AcceptedRole);
                                        return;
                                    }

                                    for (let i = 0; i < keywordList.count; i++) {
                                        let candidate = keywordSelection.index(i, 0);
                                        if (keywordSelection.data(candidate, keywordSelection.selectionRole)) {
                                            keywordSelection.setData(candidate, value, KeywordGroupByKeyModel.AcceptedRole);
                                        }
                                    }
                                }

                                QC.Menu {
                                    id: keywordContextMenu
                                    QC.MenuItem {
                                        text: qsTr("Select All")
                                        onTriggered: keywordSelection.selectAll()
                                    }
                                }

                                delegate:  TableRowBackground {
                                    id: rowId
                                    objectName: "row" + rowId.index;

                                    required property bool acceptedRole
                                    required property string valueRole
                                    required property int objectCountRole
                                    required property bool selectedRole
                                    required property int index

                                    width: keywordList.width
                                    height: rowLayoutId.height + 4

                                    rowIndex: rowId.index
                                    isSelected: rowId.selectedRole

                                    // anchors.fill: parent
                                    // color: selectedRole ? Theme.highlight : Theme.transparent
                                    // radius: 4

                                    TapHandler {
                                        acceptedButtons: Qt.LeftButton
                                        acceptedModifiers: Qt.NoModifier
                                        gesturePolicy: TapHandler.ReleaseWithinBounds
                                        onTapped: function() {
                                            keywordSelection.clearSelection();
                                            keywordSelection.setSelected(keywordSelection.index(index, 0), true);
                                            keywordList.anchorIndex = index;
                                        }
                                    }

                                    TapHandler {
                                        acceptedButtons: Qt.LeftButton
                                        acceptedModifiers: Qt.ShiftModifier
                                        gesturePolicy: TapHandler.ReleaseWithinBounds
                                        onTapped: function() {
                                            if (keywordList.anchorIndex < 0) {
                                                keywordList.anchorIndex = index;
                                            }
                                            keywordSelection.clearSelection();
                                            let start = Math.min(keywordList.anchorIndex, index);
                                            let end = Math.max(keywordList.anchorIndex, index);
                                            for (let i = start; i <= end; i++) {
                                                keywordSelection.setSelected(keywordSelection.index(i, 0), true);
                                            }
                                        }
                                    }

                                    TapHandler {
                                        acceptedButtons: Qt.LeftButton
                                        acceptedModifiers: Qt.ControlModifier
                                        gesturePolicy: TapHandler.ReleaseWithinBounds
                                        onTapped: function() {
                                            let proxyIndex = keywordSelection.index(index, 0);
                                            let current = keywordSelection.data(proxyIndex, keywordSelection.selectionRole);
                                            keywordSelection.setSelected(proxyIndex, !current);
                                            keywordList.anchorIndex = index;
                                        }
                                    }

                                    TapHandler {
                                        acceptedButtons: Qt.RightButton
                                        acceptedModifiers: Qt.NoModifier
                                        gesturePolicy: TapHandler.ReleaseWithinBounds
                                        onTapped: function(point) {
                                            keywordSelection.clearSelection();
                                            keywordSelection.setSelected(keywordSelection.index(index, 0), true);
                                            keywordList.anchorIndex = index;
                                            keywordContextMenu.popup(point.scenePosition);
                                        }
                                    }

                                    RowLayout {
                                        id: rowLayoutId

                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 4


                                        QC.CheckBox {
                                            objectName: "checkbox"
                                            checked: acceptedRole
                                            onToggled: {
                                                keywordList.applyCheckedToSelection(checked, index);
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
