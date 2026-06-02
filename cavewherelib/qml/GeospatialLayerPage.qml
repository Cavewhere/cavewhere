/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQml
import QtQuick.Controls as QC
import QtQuick.Dialogs as QD
import QtQuick.Layouts
import cavewherelib

StandardPage {
    id: geospatialLayerPage
    objectName: "geospatialLayerPage"

    readonly property bool isNarrow: width < Theme.breakpointPanelCollapse

    function addLazFiles(urls) {
        if (urls.length === 0) {
            return
        }
        RootData.lastDirectory = urls[urls.length - 1]
        RootData.region.lazLayers.addFromFiles(urls)
    }

    RemoveAskBox {
        id: removeChallengeId
        onRemove: RootData.region.lazLayers.removeAt(indexToRemove)
    }

    QD.FileDialog {
        id: lazFileDialog
        objectName: "lazFileDialog"
        title: "Add LAZ Files"
        nameFilters: [ "LAZ point clouds (*.laz *.las)", "All files (*)" ]
        currentFolder: RootData.lastDirectory
        fileMode: QD.FileDialog.OpenFiles
        onAccepted: geospatialLayerPage.addLazFiles(lazFileDialog.selectedFiles)
    }

    AddAndSearchBar {
        id: addLazBar
        objectName: "addLazBar"
        addButtonText: "Add LAZ Files"
        onAdd: lazFileDialog.open()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        spacing: Theme.sectionSpacing

        LayoutItemProxy {
            target: addLazBar
            Layout.fillWidth: true
        }

        QC.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            QQ.ListView {
                id: layerListView
                objectName: "geospatialLayerTableView"
                model: RootData.region.lazLayers
                clip: true

                delegate: geospatialLayerPage.isNarrow ? narrowDelegateComponent : wideDelegateComponent
            }
        }
    }

    HelpBox {
        objectName: "noGeospatialLayersHelpBox"
        anchors.centerIn: parent
        text: "No geospatial layers yet. Click <b>Add LAZ Files</b> to add a LiDAR point cloud."
        visible: RootData.region.lazLayers.count === 0
    }

    HelpBox {
        objectName: "noProjectCSHelpBox"
        text: "One or more layers don't have an embedded coordinate system.<br>"
              + "Set the project's coordinate system on the <b>Data</b> page to align them with surveys."
        visible: RootData.region.lazLayers.count > 0
                 && RootData.region.globalCoordinateSystem === ""
    }

    QQ.Component {
        id: wideDelegateComponent

        QQ.Item {
            id: wideDelegateId

            required property int index
            required property string name
            required property string sourceCS
            required property int pointCount
            required property LazLayer lazLayer

            implicitHeight: rowLayoutId.implicitHeight + Theme.tightSpacing * 2
            width: QQ.ListView.view ? QQ.ListView.view.width : 0

            TableRowBackground {
                isSelected: layerListView.currentIndex === wideDelegateId.index
                rowIndex: wideDelegateId.index
                anchors.fill: parent
            }

            QQ.TapHandler {
                acceptedButtons: Qt.LeftButton
                onTapped: layerListView.currentIndex = wideDelegateId.index
            }

            RowLayout {
                id: rowLayoutId
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Theme.delegatePadding
                anchors.rightMargin: Theme.delegatePadding
                spacing: Theme.columnGap

                QC.CheckBox {
                    objectName: "enabledToggle"
                    checked: wideDelegateId.lazLayer.enabled
                    onToggled: wideDelegateId.lazLayer.enabled = checked
                }

                QC.Label {
                    Layout.preferredWidth: 220
                    text: wideDelegateId.name
                    elide: QQ.Text.ElideMiddle
                    opacity: wideDelegateId.lazLayer.enabled ? 1.0 : 0.5
                }

                QC.Label {
                    Layout.preferredWidth: 140
                    text: wideDelegateId.sourceCS
                    color: Theme.textSubtle
                    opacity: wideDelegateId.lazLayer.enabled ? 1.0 : 0.5
                }

                QC.Label {
                    Layout.preferredWidth: 120
                    text: wideDelegateId.pointCount.toLocaleString() + " pts"
                    opacity: wideDelegateId.lazLayer.enabled ? 1.0 : 0.5
                }

                QC.Label {
                    objectName: "disabledChip"
                    text: "Disabled"
                    color: Theme.textSubtle
                    font.pixelSize: Theme.fontSizeCaption
                    visible: !wideDelegateId.lazLayer.enabled
                }

                QQ.Item { Layout.fillWidth: true }
            }

            DataRightClickMouseMenu {
                anchors.fill: parent
                removeChallenge: removeChallengeId
                row: wideDelegateId.index
                name: wideDelegateId.name
            }
        }
    }

    QQ.Component {
        id: narrowDelegateComponent

        QQ.Item {
            id: narrowDelegateId

            required property int index
            required property string name
            required property string sourceCS
            required property int pointCount
            required property LazLayer lazLayer

            width: QQ.ListView.view ? QQ.ListView.view.width : 0
            implicitHeight: narrowRow.implicitHeight + Theme.delegatePadding * 2

            TableRowBackground {
                isSelected: layerListView.currentIndex === narrowDelegateId.index
                rowIndex: narrowDelegateId.index
                anchors.fill: parent
            }

            QQ.TapHandler {
                acceptedButtons: Qt.LeftButton
                onTapped: layerListView.currentIndex = narrowDelegateId.index
            }

            RowLayout {
                id: narrowRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: Theme.delegatePadding
                anchors.rightMargin: Theme.delegatePadding
                spacing: Theme.columnGap

                QC.CheckBox {
                    objectName: "enabledToggle"
                    checked: narrowDelegateId.lazLayer.enabled
                    onToggled: narrowDelegateId.lazLayer.enabled = checked
                }

                QQ.Flow {
                    id: narrowFlow
                    Layout.fillWidth: true
                    spacing: Theme.flowSpacing
                    opacity: narrowDelegateId.lazLayer.enabled ? 1.0 : 0.5

                    QC.Label {
                        text: narrowDelegateId.name
                        font.bold: true
                    }

                    QC.Label { text: "·"; color: Theme.textSubtle }

                    QC.Label {
                        text: narrowDelegateId.sourceCS
                        color: Theme.textSubtle
                    }

                    QC.Label { text: "·"; color: Theme.textSubtle }

                    QC.Label {
                        text: narrowDelegateId.pointCount.toLocaleString() + " pts"
                    }
                }

                QC.Label {
                    objectName: "disabledChip"
                    text: "Disabled"
                    color: Theme.textSubtle
                    font.pixelSize: Theme.fontSizeCaption
                    visible: !narrowDelegateId.lazLayer.enabled
                }
            }

            DataRightClickMouseMenu {
                anchors.fill: parent
                removeChallenge: removeChallengeId
                row: narrowDelegateId.index
                name: narrowDelegateId.name
            }
        }
    }
}
