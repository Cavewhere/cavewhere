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

QC.Popup {
    id: root

    required property CoordinatePicker picker

    readonly property int _gap: 12
    readonly property int _coordPrecision: 3
    readonly property int _wgsPrecision: 6
    readonly property int _messageWidth: 240

    // True when the project has nothing to anchor its local projection to, so the
    // pick can't be placed in real-world coordinates.
    readonly property bool _needsCoordinateSystem: !picker.hasCoordinateSystem

    // Send the user to the Data page, where a fix station or a geospatial layer
    // gives the project its position. cwLinkGenerator owns the page-address
    // scheme so this leaf doesn't hardcode the page tree.
    function _gotoCoordinateSystem() {
        root.picker.clearPick()
        RootData.pageSelectionModel.currentPageAddress = linkGeneratorId.dataPageLink()
    }

    function _formatLatLon(lat, lon) {
        return "%1, %2"
            .arg(Number(lat).toFixed(root._wgsPrecision))
            .arg(Number(lon).toFixed(root._wgsPrecision))
    }

    component CopySection: ColumnLayout {
        id: sectionId
        required property string headerText
        required property string valueText
        property string objectNameRoot: ""

        Layout.fillWidth: true
        spacing: 2

        QC.Label {
            text: sectionId.headerText
            color: Theme.textSecondary
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            QC.TextField {
                id: valueFieldId
                objectName: sectionId.objectNameRoot + "Field"
                Layout.fillWidth: true
                readOnly: true
                selectByMouse: true
                font.family: Theme.fontFamilyMono
                text: sectionId.valueText
            }

            QC.ToolButton {
                objectName: "copy" + sectionId.objectNameRoot
                text: qsTr("Copy")
                onClicked: RootData.copyText(valueFieldId.text)
            }
        }
    }

    padding: 12
    closePolicy: QC.Popup.CloseOnEscape | QC.Popup.CloseOnPressOutsideParent
    modal: false

    x: parent
       ? Math.max(0, Math.min(picker.pickScreenPoint.x + root._gap,
                              parent.width  - root.width))
       : 0
    y: parent
       ? Math.max(0, Math.min(picker.pickScreenPoint.y + root._gap,
                              parent.height - root.height))
       : 0

    background: QQ.Rectangle {
        color: Theme.surfaceRaised
        border.color: Theme.border
        border.width: 1
        radius: Theme.floatingWidgetRadius
    }

    contentItem: ColumnLayout {
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            QC.Label {
                text: qsTr("Picked coordinates")
                color: Theme.text
                font.bold: true
                Layout.fillWidth: true
            }

            QC.ToolButton {
                objectName: "closeButton"
                text: qsTr("×")
                font.pixelSize: Theme.fontSizeMedium
                onClicked: root.picker.clearPick()
            }
        }

        ColumnLayout {
            objectName: "needsCoordinateSystem"
            visible: root._needsCoordinateSystem
            Layout.fillWidth: true
            Layout.maximumWidth: root._messageWidth
            spacing: 6

            QC.Label {
                Layout.fillWidth: true
                wrapMode: QQ.Text.WordWrap
                color: Theme.text
                text: qsTr("This project isn't positioned yet, so the pick can't be shown in real-world coordinates.")
            }

            LinkText {
                objectName: "coordinateSystemLink"
                text: qsTr("Add a fix station or a geospatial layer")
                onClicked: root._gotoCoordinateSystem()
            }
        }

        CopySection {
            visible: root.picker.hasWgs84
            objectNameRoot: "Wgs"
            headerText: qsTr("WGS84 (lat, lon)")
            valueText: root._formatLatLon(root.picker.wgs84Latitude,
                                          root.picker.wgs84Longitude)
        }

        CopySection {
            visible: root.picker.hasCoordinateSystem
            objectNameRoot: "Elev"
            headerText: qsTr("Elevation")
            valueText: "%1 m".arg(Number(root.picker.elevation).toFixed(root._coordPrecision))
        }
    }

    LinkGenerator {
        id: linkGeneratorId
        pageSelectionModel: RootData.pageSelectionModel
    }
}
