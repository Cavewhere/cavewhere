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

ColumnLayout {
    id: rootId
    objectName: "colorByOptions"

    readonly property ColorBy colorBy: RootData.regionSceneManager.colorBy
    readonly property bool active: colorBy.source !== ColorBy.None
    readonly property int rowSpacing: 4
    // Meters; wide enough for georeferenced elevations
    readonly property int pivotLimit: 10000

    spacing: rowSpacing

    QC.Label {
        text: qsTr("Color by")
        font.pixelSize: Theme.fontSizeUI
    }

    QC.ComboBox {
        id: sourceComboId
        objectName: "colorBySourceCombo"

        Layout.fillWidth: true
        model: [qsTr("None"), qsTr("Depth")]

        currentIndex: rootId.colorBy.source === ColorBy.Depth ? 1 : 0

        onActivated: (index) => rootId.colorBy.source = index === 1 ? ColorBy.Depth : ColorBy.None
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

        visible: rootId.active
        displayText: rootId.colorBy.rampName(currentText)
        currentIndex: rootId.colorBy.rampIds.indexOf(rootId.colorBy.rampId)

        onActivated: (index) => rootId.colorBy.rampId = rootId.colorBy.rampIds[index]
    }

    RowLayout {
        visible: rootId.active

        QC.Label {
            text: qsTr("White at")
        }

        QC.SpinBox {
            objectName: "colorByPivotSpin"

            from: -rootId.pivotLimit
            to: rootId.pivotLimit

            value: Math.round(rootId.colorBy.pivot)

            onValueModified: rootId.colorBy.pivot = value
        }

        QC.Label {
            text: qsTr("m")
        }
    }

    RowLayout {
        visible: rootId.active

        QC.Label {
            text: qsTr("Scrap tint")
        }

        QC.Slider {
            objectName: "colorByStrengthSlider"

            Layout.fillWidth: true
            from: 0
            to: 1

            value: rootId.colorBy.scrapStrength

            onMoved: rootId.colorBy.scrapStrength = value
        }
    }
}
