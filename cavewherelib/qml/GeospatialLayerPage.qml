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

    // Column widths shared between the static header and the wide delegate so
    // every column lines up.
    readonly property real nameColumnWidth: 220
    readonly property real csColumnWidth: 180
    readonly property real pointCountColumnWidth: 120

    function addLazFiles(urls) {
        if (urls.length === 0) {
            return
        }
        RootData.lastDirectory = urls[urls.length - 1]
        RootData.region.lazLayers.addFromFiles(urls)
    }

    RemoveAskBox {
        id: removeChallengeId
        // The proxy reorders rows but the source model owns the data; removeAt
        // is keyed on the source row. Map back through the proxy so the
        // delegate's proxy-row index lands on the right layer.
        onRemove: {
            const sourceIndex = lazLayersProxyModel.mapToSource(
                        lazLayersProxyModel.index(indexToRemove, 0))
            RootData.region.lazLayers.removeAt(sourceIndex.row)
        }
    }

    SortFilterProxyModel {
        id: lazLayersProxyModel
        source: RootData.region.lazLayers
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

        RowLayout {
            id: headerRow
            objectName: "geospatialLayerHeader"
            visible: !geospatialLayerPage.isNarrow
                     && RootData.region.lazLayers.count > 0
            Layout.fillWidth: true
            spacing: Theme.columnGap

            TableStaticHeaderColumn {
                objectName: "nameHeader"
                Layout.preferredWidth: geospatialLayerPage.nameColumnWidth
                columnWidth: geospatialLayerPage.nameColumnWidth
                text: "Name"
                sortRole: LazLayerModel.NameRole
                model: lazLayersProxyModel
            }

            TableStaticHeaderColumn {
                objectName: "csHeader"
                Layout.preferredWidth: geospatialLayerPage.csColumnWidth
                columnWidth: geospatialLayerPage.csColumnWidth
                text: "Coordinate System"
                sortRole: LazLayerModel.SourceCSDisplayNameRole
                model: lazLayersProxyModel
            }

            TableStaticHeaderColumn {
                objectName: "pointsHeader"
                Layout.preferredWidth: geospatialLayerPage.pointCountColumnWidth
                columnWidth: geospatialLayerPage.pointCountColumnWidth
                text: "Points"
                sortRole: LazLayerModel.PointCountRole
                model: lazLayersProxyModel
            }

            QQ.Item { Layout.fillWidth: true }
        }

        QC.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            QQ.ListView {
                id: layerListView
                objectName: "geospatialLayerTableView"
                model: lazLayersProxyModel
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
            required property string sourceCSDisplayName
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

                QC.Label {
                    Layout.preferredWidth: geospatialLayerPage.nameColumnWidth
                    text: wideDelegateId.name
                    elide: QQ.Text.ElideMiddle
                    opacity: wideDelegateId.lazLayer.enabled ? 1.0 : 0.5
                }

                QC.Label {
                    id: csCellLabel
                    objectName: "sourceCSCell"
                    Layout.preferredWidth: geospatialLayerPage.csColumnWidth
                    // sourceCSDisplayName is the proj-extracted human name
                    // ("NAD83 / UTM zone 13N"); sourceCS is the raw WKT or
                    // EPSG string. Show the short form in the cell, full form
                    // in the tooltip so users can verify the projection
                    // details without overwhelming the row.
                    text: wideDelegateId.sourceCSDisplayName
                    color: Theme.textSubtle
                    opacity: wideDelegateId.lazLayer.enabled ? 1.0 : 0.5
                    elide: QQ.Text.ElideRight

                    QQ.HoverHandler { id: csCellHover }

                    QC.ToolTip.visible: csCellHover.hovered
                                        && wideDelegateId.sourceCS.length > 0
                    // Gate the text on hover so the binding doesn't re-pull
                    // the full WKT for every delegate on every hover event
                    // elsewhere in the list.
                    QC.ToolTip.text: csCellHover.hovered ? wideDelegateId.sourceCS : ""
                    QC.ToolTip.delay: 500
                }

                QC.Label {
                    Layout.preferredWidth: geospatialLayerPage.pointCountColumnWidth
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

            LazLayerContextMenu {
                anchors.fill: parent
                removeChallenge: removeChallengeId
                row: wideDelegateId.index
                name: wideDelegateId.name
                lazLayer: wideDelegateId.lazLayer
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
            required property string sourceCSDisplayName
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
                        id: narrowCSLabel
                        objectName: "sourceCSCell"
                        // Same short-name / full-WKT split as the wide
                        // delegate; the Flow row stays readable on phones.
                        text: narrowDelegateId.sourceCSDisplayName
                        color: Theme.textSubtle

                        QQ.HoverHandler { id: narrowCSHover }

                        QC.ToolTip.visible: narrowCSHover.hovered
                                            && narrowDelegateId.sourceCS.length > 0
                        QC.ToolTip.text: narrowCSHover.hovered ? narrowDelegateId.sourceCS : ""
                        QC.ToolTip.delay: 500
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

            LazLayerContextMenu {
                anchors.fill: parent
                removeChallenge: removeChallengeId
                row: narrowDelegateId.index
                name: narrowDelegateId.name
                lazLayer: narrowDelegateId.lazLayer
            }
        }
    }
}
