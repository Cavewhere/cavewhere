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

GridLayout {
    id: rootId
    objectName: "colorByOptions"

    readonly property ColorBy colorBy: RootData.regionSceneManager.colorBy
    readonly property bool active: colorBy.source !== ColorBy.None
    readonly property bool depthActive: colorBy.source === ColorBy.Depth
    // Scraps carry no loop error, so their tint is off in that mode
    readonly property bool scrapTintActive: active && colorBy.source !== ColorBy.LoopError
    readonly property list<string> keywordKeys: RootData.keywordFilterPipelineModel.possibleKeys
        .filter(key => !colorBy.hiddenKeywordKeys.includes(key))
    // Source combo rows: None, Depth, Loop error, then the keys
    readonly property int depthIndex: 1
    readonly property int loopErrorIndex: 2
    readonly property int firstKeyIndex: 3
    // Whole meters or feet; covers about 9 km either way in both units
    readonly property int pivotLimit: 30000
    readonly property int depthUnit: Units.depthDisplayUnit(ProjectUnits.unitSystem)
    readonly property string depthUnitName: Units.lengthUnitName(depthUnit)

    columns: 2
    columnSpacing: Theme.delegatePadding
    rowSpacing: Theme.tightSpacing

    component RowLabel: QC.Label {
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
    }

    RowLabel {
        text: qsTr("Color by")
    }

    QC.ComboBox {
        id: sourceComboId
        objectName: "colorBySourceCombo"

        Layout.fillWidth: true
        model: [qsTr("None"), qsTr("Depth"), qsTr("Loop error")].concat(rootId.keywordKeys)

        currentIndex: {
            switch (rootId.colorBy.source) {
            case ColorBy.Depth:
                return rootId.depthIndex;
            case ColorBy.LoopError:
                return rootId.loopErrorIndex;
            case ColorBy.Keyword: {
                // A stored key the project lacks selects nothing
                const keyIndex = rootId.keywordKeys.indexOf(rootId.colorBy.keywordKey);
                return keyIndex >= 0 ? rootId.firstKeyIndex + keyIndex : -1;
            }
            default:
                return 0;
            }
        }

        onActivated: (index) => {
            if (index >= rootId.firstKeyIndex) {
                rootId.colorBy.keywordKey = rootId.keywordKeys[index - rootId.firstKeyIndex];
                rootId.colorBy.source = ColorBy.Keyword;
            } else if (index === rootId.depthIndex) {
                rootId.colorBy.source = ColorBy.Depth;
            } else if (index === rootId.loopErrorIndex) {
                rootId.colorBy.source = ColorBy.LoopError;
            } else {
                rootId.colorBy.source = ColorBy.None;
            }
        }
    }

    RowLabel {
        visible: rootId.depthActive
        text: qsTr("Palette")
    }

    QC.ComboBox {
        id: rampComboId
        objectName: "colorByRampCombo"

        Layout.fillWidth: true
        model: rootId.colorBy.rampIds
        delegate: QC.ItemDelegate {
            required property string modelData
            width: rampComboId.width
            text: rootId.colorBy.rampName(modelData)
        }

        visible: rootId.depthActive
        displayText: rootId.colorBy.rampName(currentText)
        currentIndex: rootId.colorBy.rampIds.indexOf(rootId.colorBy.rampId)

        onActivated: (index) => rootId.colorBy.rampId = rootId.colorBy.rampIds[index]
    }

    RowLabel {
        visible: rootId.depthActive
        text: qsTr("White at")
    }

    RowLayout {
        Layout.fillWidth: true
        visible: rootId.depthActive

        QC.SpinBox {
            objectName: "colorByPivotSpin"

            Layout.fillWidth: true
            editable: true
            from: -rootId.pivotLimit
            to: rootId.pivotLimit

            // pivot is stored in meters; the spin shows the project's depth unit
            value: Math.round(Units.convertLength(rootId.colorBy.pivot, Units.Meters, rootId.depthUnit))

            onValueModified: rootId.colorBy.pivotManual = Units.convertLength(value, rootId.depthUnit, Units.Meters)
        }

        QC.Label {
            objectName: "colorByPivotUnit"
            text: rootId.depthUnitName
        }

        // Back to the middle of the depth range
        QC.Button {
            id: pivotAutoButtonId
            objectName: "colorByPivotAutoButton"

            visible: rootId.colorBy.hasPivotManual
            flat: true
            text: qsTr("Auto")

            onClicked: rootId.colorBy.clearPivotManual()

            contentItem: QC.Label {
                text: pivotAutoButtonId.text
                color: Theme.textLink
            }
        }
    }

    RowLabel {
        visible: rootId.scrapTintActive
        text: qsTr("Scrap tint")
    }

    QC.Slider {
        objectName: "colorByStrengthSlider"

        Layout.fillWidth: true
        visible: rootId.scrapTintActive
        from: 0
        to: 1

        value: rootId.colorBy.scrapStrength

        onMoved: rootId.colorBy.scrapStrength = value
    }
}
