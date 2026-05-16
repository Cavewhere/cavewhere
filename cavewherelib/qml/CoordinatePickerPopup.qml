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

    // Cache the CRS header so binding evaluations during pick changes don't
    // re-run the PROJ name lookup + qsTr interpolation each time.
    readonly property string _crsHeader: {
        if (picker.globalCS === "") {
            return ""
        }
        const name = CoordinateSystem.nameFor(picker.globalCS)
        return qsTr("Project CRS — %1").arg(name !== "" ? name : picker.globalCS)
    }

    function _formatXYZ(x, y, z, precision) {
        return "%1, %2, %3"
            .arg(Number(x).toFixed(precision))
            .arg(Number(y).toFixed(precision))
            .arg(Number(z).toFixed(precision))
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

        CopySection {
            visible: root.picker.globalCS !== ""
            objectNameRoot: "CS"
            headerText: root._crsHeader
            valueText: root._formatXYZ(root.picker.csX,
                                       root.picker.csY,
                                       root.picker.csZ,
                                       root._coordPrecision)
        }

        CopySection {
            visible: root.picker.hasWgs84
            objectNameRoot: "Wgs"
            headerText: qsTr("WGS84 (lat, lon)")
            valueText: root._formatLatLon(root.picker.wgs84Latitude,
                                          root.picker.wgs84Longitude)
        }

        CopySection {
            visible: root.picker.globalCS !== ""
            objectNameRoot: "Elev"
            headerText: qsTr("Elevation")
            valueText: "%1 m".arg(Number(root.picker.elevation).toFixed(root._coordPrecision))
        }
    }
}
