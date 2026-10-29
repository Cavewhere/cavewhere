/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// The marker knobs: depth test, x-ray and x-ray opacity for entrance markers,
// opened from the render stats HUD beside the point cloud knobs. Each row writes
// RenderingSettings, and the entrance markers redraw at once. Leads always draw
// depth tested with no x-ray.
QC.Popup {
    id: panelId
    objectName: "markerDebugPanel"

    property RenderingSettings renderingSettings: RootData.settings.renderingSettings

    // Left open while the camera orbits, with key focus left on the 3D view so
    // its shortcuts keep working: only Escape or Close shuts it.
    modal: false
    focus: false
    padding: Theme.statsPadding
    closePolicy: QC.Popup.CloseOnEscape

    contentItem: ColumnLayout {
        spacing: Theme.tightSpacing

        PointCloudDebugPanel.KnobCheckBox {
            objectName: "markerDepthTestCheckBox"
            text: qsTr("Depth test")
            knobValue: panelId.renderingSettings.markerDepthTest
            onToggled: panelId.renderingSettings.markerDepthTest = checked
        }

        // X-ray draws only while the depth test is on.
        PointCloudDebugPanel.KnobCheckBox {
            objectName: "markerXRayCheckBox"
            text: qsTr("X-ray hidden entrances")
            enabled: panelId.renderingSettings.markerDepthTest
            knobValue: panelId.renderingSettings.markerXRay
            onToggled: panelId.renderingSettings.markerXRay = checked
        }

        RowLayout {
            QC.Label {
                text: qsTr("X-ray opacity (%)")
                font.pixelSize: Theme.fontSizeCaption
                enabled: panelId.renderingSettings.markerDepthTest
            }

            PointCloudDebugPanel.KnobSpinBox {
                objectName: "markerXRayOpacitySpinBox"
                from: panelId.renderingSettings.minimumMarkerXRayOpacityPercent
                to: panelId.renderingSettings.maximumMarkerXRayOpacityPercent
                enabled: panelId.renderingSettings.markerDepthTest
                knobValue: panelId.renderingSettings.markerXRayOpacityPercent
                onKnobEdited: (editedValue) => panelId.renderingSettings.markerXRayOpacityPercent = editedValue
            }
        }

        RowLayout {
            Layout.topMargin: Theme.sectionSpacing
            Layout.alignment: Qt.AlignRight

            QC.Button {
                objectName: "markerDebugResetButton"
                text: qsTr("Reset to defaults")
                onClicked: panelId.renderingSettings.resetMarkerKnobsToDefaults()
            }

            QC.Button {
                objectName: "markerDebugCloseButton"
                text: qsTr("Close")
                onClicked: panelId.close()
            }
        }
    }
}
