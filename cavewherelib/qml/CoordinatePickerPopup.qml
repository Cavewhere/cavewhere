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
    // 8 decimals of a degree is ~1 mm of latitude, matching the millimeter the
    // elevation beside it already reads to (cwUnits::lengthDecimals). The 7th
    // holds latitude to ~11 mm, coarser than the scan under the pick; the float
    // the pick arrives as runs out around the 9th.
    readonly property int _wgsPrecision: 8
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

    // The elevation in the project's unit system, suffixed with that unit.
    // depthDisplayUnit is the m/ft pick; lengthDisplayUnit would roll a 2200 m
    // elevation over to km. Reading ProjectUnits.unitSystem here is what re-runs
    // the binding when the project switches systems.
    function _formatElevation(elevationInMeters) {
        const unit = Units.depthDisplayUnit(ProjectUnits.unitSystem)
        const elevation = Units.convertLength(elevationInMeters, Units.Meters, unit)
        return elevation.toFixed(Units.lengthDecimals(unit)) + Units.lengthUnitName(unit)
    }

    // Latitude, longitude and elevation as one comma-separated triple, so a
    // single copy carries the full 3D position. Only the elevation carries a
    // unit — degrees are degrees.
    function _formatLatLonElevation(lat, lon, elevationInMeters) {
        return "%1, %2, %3"
            .arg(lat.toFixed(root._wgsPrecision))
            .arg(lon.toFixed(root._wgsPrecision))
            .arg(root._formatElevation(elevationInMeters))
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

                // The style's background gives the field a fixed implicit width
                // that's narrower than a lat/lon/elevation triple, which clips the
                // value. Grow the layout slot to the text instead, with a margin so
                // sub-pixel rounding on a HiDPI display can't shave off a digit.
                readonly property int _clipMargin: 6
                readonly property real _slotWidth: valueFieldId.contentWidth
                                                   + valueFieldId.leftPadding
                                                   + valueFieldId.rightPadding
                                                   + valueFieldId._clipMargin

                Layout.fillWidth: true
                Layout.preferredWidth: valueFieldId._slotWidth
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
            headerText: qsTr("WGS84 (lat, lon, elevation)")
            valueText: root._formatLatLonElevation(root.picker.wgs84Latitude,
                                                   root.picker.wgs84Longitude,
                                                   root.picker.elevation)
        }

        // A pick with a coordinate system whose WGS84 transform failed to build
        // still has a height to report — show it alone rather than an empty popup.
        CopySection {
            visible: root.picker.hasCoordinateSystem && !root.picker.hasWgs84
            objectNameRoot: "Elev"
            headerText: qsTr("Elevation")
            valueText: root._formatElevation(root.picker.elevation)
        }
    }

    LinkGenerator {
        id: linkGeneratorId
        pageSelectionModel: RootData.pageSelectionModel
    }
}
