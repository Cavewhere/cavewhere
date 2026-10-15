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

// Every streaming and CLOD knob of the point cloud renderer in one popover, so
// an A/B is a click away while the camera holds still. Each row writes
// RenderingSettings (spacingCoverage writes the scene node the P + wheel
// gesture drives), and the node and point counts sit beside them so the cost
// of a change shows next to it.
QC.Popup {
    id: panelId
    objectName: "pointCloudDebugPanel"

    property RenderingSettings renderingSettings: RootData.settings.renderingSettings
    property LazLayersSceneNode lazLayersSceneNode: RootData.regionSceneManager.lazLayersSceneNode

    readonly property list<string> thinningModeNames: [qsTr("Continuous"), qsTr("Discrete"), qsTr("Off")]
    readonly property list<string> sizeModeNames: [qsTr("Coverage"), qsTr("Fixed pixels"), qsTr("Own spacing")]
    readonly property list<string> floorModeNames: [qsTr("Node"), qsTr("Off")]
    readonly property list<string> spacingSourceNames: [qsTr("Grid"), qsTr("Measured")]
    readonly property list<string> debugViewNames: [qsTr("Off"), qsTr("Level"), qsTr("Node"), qsTr("Rank"),
        qsTr("Weight"), qsTr("Sprite size"), qsTr("Floor ratio"), qsTr("Residency age"),
        qsTr("Decimation preview")]

    // QC.SpinBox counts in integers, so fractional knobs are held scaled.
    readonly property int screenSpaceErrorScale: 10
    readonly property int screenSpaceErrorDecimals: 1
    readonly property int spacingCoverageScale: 100
    readonly property int spacingCoverageDecimals: 2
    readonly property int governorSlewScale: 100
    readonly property int governorSlewDecimals: 2
    readonly property int labelColumnWidth: Math.round(150 * Theme.fontScale)

    // Left open while the camera orbits, with key focus left on the 3D view so
    // its T, V and F shortcuts keep working: only Escape or Close shuts it.
    modal: false
    focus: false
    padding: Theme.statsPadding
    closePolicy: QC.Popup.CloseOnEscape

    function resetToDefaults() {
        panelId.renderingSettings.resetPointCloudKnobsToDefaults()
        if (panelId.lazLayersSceneNode) {
            panelId.lazLayersSceneNode.spacingCoverage = panelId.lazLayersSceneNode.defaultSpacingCoverage
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.tightSpacing

        QC.Label {
            objectName: "pointCloudDebugCounts"
            text: qsTr("%1 / %2 nodes · %3 loading · points %4 / %5 M")
                .arg(statsModelId.residentNodes)
                .arg(statsModelId.selectedNodes)
                .arg(statsModelId.nodeLoadsInFlight)
                .arg(statsModelId.selectedPointsText)
                .arg(panelId.renderingSettings.pointBudgetMillions)
            font.pixelSize: Theme.fontSizeCaption
        }

        SectionLabel { text: qsTr("Streaming") }

        RowLayout {
            KnobCheckBox {
                objectName: "freezeCutCheckBox"
                text: qsTr("Freeze cut (F)")
                knobValue: panelId.renderingSettings.freezeCut
                onToggled: panelId.renderingSettings.freezeCut = checked
            }

            KnobCheckBox {
                objectName: "inflationGovernorCheckBox"
                text: qsTr("Inflation governor")
                knobValue: panelId.renderingSettings.inflationGovernor
                onToggled: panelId.renderingSettings.inflationGovernor = checked
            }

            KnobCheckBox {
                objectName: "evictionHoldCheckBox"
                text: qsTr("Eviction hold")
                knobValue: panelId.renderingSettings.evictionHold
                onToggled: panelId.renderingSettings.evictionHold = checked
            }
        }

        RowLayout {
            KnobLabel { text: qsTr("Governor slew") }

            KnobSpinBox {
                objectName: "governorSlewSpinBox"
                enabled: panelId.renderingSettings.inflationGovernor
                valueScale: panelId.governorSlewScale
                decimals: panelId.governorSlewDecimals
                from: Math.round(panelId.renderingSettings.minimumGovernorSlew * panelId.governorSlewScale)
                to: Math.round(panelId.renderingSettings.maximumGovernorSlew * panelId.governorSlewScale)
                knobValue: panelId.renderingSettings.governorSlew
                onKnobEdited: (editedValue) => panelId.renderingSettings.governorSlew = editedValue
            }
        }

        RowLayout {
            KnobLabel { text: qsTr("Max level") }

            KnobSpinBox {
                objectName: "maxLevelSpinBox"
                from: panelId.renderingSettings.unlimitedLevel
                to: panelId.renderingSettings.maximumLevel
                knobValue: panelId.renderingSettings.maxLevel
                textFromValue: function(value, locale) {
                    return value === panelId.renderingSettings.unlimitedLevel
                        ? qsTr("Unlimited")
                        : Number(value).toLocaleString(locale, 'f', 0)
                }
                valueFromText: function(text, locale) {
                    return text === qsTr("Unlimited")
                        ? panelId.renderingSettings.unlimitedLevel
                        : Math.round(Number.fromLocaleString(locale, text))
                }
                onKnobEdited: (editedValue) => panelId.renderingSettings.maxLevel = editedValue
            }
        }

        RowLayout {
            KnobLabel { text: qsTr("Min level") }

            KnobSpinBox {
                objectName: "minLevelSpinBox"
                from: panelId.renderingSettings.rootLevel
                to: panelId.renderingSettings.maximumLevel
                knobValue: panelId.renderingSettings.minLevel
                onKnobEdited: (editedValue) => panelId.renderingSettings.minLevel = editedValue
            }
        }

        SectionLabel { text: qsTr("Debug views") }

        RowLayout {
            KnobLabel { text: qsTr("Color by (V)") }

            KnobComboBox {
                objectName: "debugViewComboBox"
                model: panelId.debugViewNames
                knobValue: panelId.renderingSettings.debugView
                onActivated: (index) => panelId.renderingSettings.debugView = index
            }
        }

        KnobCheckBox {
            objectName: "showNodeBoxesCheckBox"
            text: qsTr("Node boxes")
            knobValue: panelId.renderingSettings.showNodeBoxes
            onToggled: panelId.renderingSettings.showNodeBoxes = checked
        }

        SectionLabel { text: qsTr("Budgets") }

        RowLayout {
            KnobLabel { text: qsTr("GPU memory (MB)") }

            KnobSpinBox {
                objectName: "gpuMemoryBudgetSpinBox"
                from: panelId.renderingSettings.minimumGpuMemoryBudgetMb
                to: panelId.renderingSettings.maximumGpuMemoryBudgetMb
                knobValue: panelId.renderingSettings.gpuMemoryBudgetMb
                onKnobEdited: (editedValue) => panelId.renderingSettings.gpuMemoryBudgetMb = editedValue
            }
        }

        RowLayout {
            KnobLabel { text: qsTr("CPU cache (MB)") }

            KnobSpinBox {
                objectName: "cpuCacheBudgetSpinBox"
                from: panelId.renderingSettings.minimumCpuCacheBudgetMb
                to: panelId.renderingSettings.maximumCpuCacheBudgetMb
                knobValue: panelId.renderingSettings.cpuCacheBudgetMb
                onKnobEdited: (editedValue) => panelId.renderingSettings.cpuCacheBudgetMb = editedValue
            }
        }

        RowLayout {
            KnobLabel { text: qsTr("Upload per frame (MB)") }

            KnobSpinBox {
                objectName: "uploadBudgetSpinBox"
                from: panelId.renderingSettings.minimumUploadBudgetMbPerFrame
                to: panelId.renderingSettings.maximumUploadBudgetMbPerFrame
                knobValue: panelId.renderingSettings.uploadBudgetMbPerFrame
                onKnobEdited: (editedValue) => panelId.renderingSettings.uploadBudgetMbPerFrame = editedValue
            }
        }

        RowLayout {
            KnobLabel { text: qsTr("Points per frame (M)") }

            KnobSpinBox {
                objectName: "pointBudgetSpinBox"
                from: panelId.renderingSettings.minimumPointBudgetMillions
                to: panelId.renderingSettings.maximumPointBudgetMillions
                knobValue: panelId.renderingSettings.pointBudgetMillions
                onKnobEdited: (editedValue) => panelId.renderingSettings.pointBudgetMillions = editedValue
            }
        }

        RowLayout {
            KnobLabel { text: qsTr("Screen-space error (px)") }

            KnobSpinBox {
                objectName: "screenSpaceErrorSpinBox"
                valueScale: panelId.screenSpaceErrorScale
                decimals: panelId.screenSpaceErrorDecimals
                from: Math.round(panelId.renderingSettings.minimumScreenSpaceErrorPx * panelId.screenSpaceErrorScale)
                to: Math.round(panelId.renderingSettings.maximumScreenSpaceErrorPx * panelId.screenSpaceErrorScale)
                knobValue: panelId.renderingSettings.screenSpaceErrorPx
                onKnobEdited: (editedValue) => panelId.renderingSettings.screenSpaceErrorPx = editedValue
            }
        }

        SectionLabel { text: qsTr("Thinning and sizing") }

        RowLayout {
            KnobLabel { text: qsTr("Thinning (T)") }

            KnobComboBox {
                objectName: "thinningModeComboBox"
                model: panelId.thinningModeNames
                knobValue: panelId.renderingSettings.thinningMode
                onActivated: (index) => panelId.renderingSettings.thinningMode = index
            }
        }

        RowLayout {
            KnobLabel { text: qsTr("Size") }

            KnobComboBox {
                objectName: "sizeModeComboBox"
                model: panelId.sizeModeNames
                knobValue: panelId.renderingSettings.sizeMode
                onActivated: (index) => panelId.renderingSettings.sizeMode = index
            }
        }

        RowLayout {
            KnobLabel { text: qsTr("Fixed point size (px)") }

            KnobSpinBox {
                objectName: "fixedPointSizeSpinBox"
                from: panelId.renderingSettings.minimumPointSizePx
                to: panelId.renderingSettings.maximumPointSizePx
                knobValue: panelId.renderingSettings.fixedPointSizePx
                onKnobEdited: (editedValue) => panelId.renderingSettings.fixedPointSizePx = editedValue
            }
        }

        RowLayout {
            KnobLabel { text: qsTr("Floor") }

            KnobComboBox {
                objectName: "floorModeComboBox"
                model: panelId.floorModeNames
                knobValue: panelId.renderingSettings.floorMode
                onActivated: (index) => panelId.renderingSettings.floorMode = index
            }
        }

        RowLayout {
            KnobLabel { text: qsTr("Spacing source") }

            KnobComboBox {
                objectName: "spacingSourceComboBox"
                model: panelId.spacingSourceNames
                knobValue: panelId.renderingSettings.spacingSource
                onActivated: (index) => panelId.renderingSettings.spacingSource = index
            }
        }

        RowLayout {
            KnobLabel { text: qsTr("Spacing coverage (P + wheel)") }

            KnobSpinBox {
                objectName: "spacingCoverageSpinBox"
                enabled: panelId.lazLayersSceneNode !== null
                valueScale: panelId.spacingCoverageScale
                decimals: panelId.spacingCoverageDecimals
                from: panelId.lazLayersSceneNode
                      ? Math.round(panelId.lazLayersSceneNode.minimumSpacingCoverage * panelId.spacingCoverageScale)
                      : 0
                to: panelId.lazLayersSceneNode
                    ? Math.round(panelId.lazLayersSceneNode.maximumSpacingCoverage * panelId.spacingCoverageScale)
                    : 0
                knobValue: panelId.lazLayersSceneNode ? panelId.lazLayersSceneNode.spacingCoverage : 0
                onKnobEdited: (editedValue) => panelId.lazLayersSceneNode.spacingCoverage = editedValue
            }
        }

        RowLayout {
            KnobLabel { text: qsTr("Max point size (px)") }

            KnobSpinBox {
                objectName: "maxPointSizeSpinBox"
                from: panelId.renderingSettings.minimumPointSizePx
                to: panelId.renderingSettings.maximumPointSizePx
                knobValue: panelId.renderingSettings.maxPointSizePx
                onKnobEdited: (editedValue) => panelId.renderingSettings.maxPointSizePx = editedValue
            }
        }

        RowLayout {
            Layout.topMargin: Theme.sectionSpacing
            Layout.alignment: Qt.AlignRight

            QC.Button {
                objectName: "pointCloudDebugResetButton"
                text: qsTr("Reset to defaults")
                onClicked: panelId.resetToDefaults()
            }

            QC.Button {
                objectName: "pointCloudDebugCloseButton"
                text: qsTr("Close")
                onClicked: panelId.close()
            }
        }
    }

    RenderingStatsModel {
        id: statsModelId
        running: panelId.visible
    }

    // A spin box over a settings value: it follows the setting (Reset, the
    // keyboard shortcuts, another view) and writes back only what the user edits.
    component KnobSpinBox: QC.SpinBox {
        id: knobSpinBoxId

        property real knobValue: 0
        property int valueScale: 1
        property int decimals: 0

        signal knobEdited(real editedValue)

        editable: true
        font.pixelSize: Theme.fontSizeCaption

        textFromValue: function(value, locale) {
            return Number(value / knobSpinBoxId.valueScale).toLocaleString(locale, 'f', knobSpinBoxId.decimals)
        }

        valueFromText: function(text, locale) {
            return Math.round(Number.fromLocaleString(locale, text) * knobSpinBoxId.valueScale)
        }

        onKnobValueChanged: value = Math.round(knobValue * valueScale)
        onValueModified: knobEdited(value / valueScale)
        QQ.Component.onCompleted: value = Math.round(knobValue * valueScale)
    }

    component KnobComboBox: QC.ComboBox {
        property int knobValue: 0

        font.pixelSize: Theme.fontSizeCaption

        onKnobValueChanged: currentIndex = knobValue
        QQ.Component.onCompleted: currentIndex = knobValue
    }

    component KnobCheckBox: QC.CheckBox {
        property bool knobValue: false

        font.pixelSize: Theme.fontSizeCaption

        onKnobValueChanged: checked = knobValue
        QQ.Component.onCompleted: checked = knobValue
    }

    component KnobLabel: QC.Label {
        font.pixelSize: Theme.fontSizeCaption
        Layout.preferredWidth: panelId.labelColumnWidth
    }

    component SectionLabel: QC.Label {
        font.pixelSize: Theme.fontSizeSmall
        font.bold: true
        Layout.topMargin: Theme.sectionSpacing
    }
}
