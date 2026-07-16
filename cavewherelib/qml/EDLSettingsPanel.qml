import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

ColumnLayout {
    id: itemId

    property Scene scene

    spacing: 4

    component LabeledSlider : RowLayout {
        id: rowId

        property alias label: labelId.text
        property alias from: sliderId.from
        property alias to: sliderId.to
        property alias stepSize: sliderId.stepSize
        property real value
        property int decimals: 2

        signal moved(real newValue)

        Layout.fillWidth: true

        QC.Label {
            id: labelId
            Layout.preferredWidth: 96
            font.pixelSize: Theme.fontSizeSmall
        }

        QC.Slider {
            id: sliderId
            Layout.fillWidth: true
            value: rowId.value
            onMoved: rowId.moved(value)
        }

        QC.Label {
            Layout.preferredWidth: 48
            horizontalAlignment: Text.AlignRight
            font.pixelSize: Theme.fontSizeSmall
            text: rowId.value.toFixed(rowId.decimals)
        }
    }

    LabeledSlider {
        objectName: "edlStrengthSlider"
        label: qsTr("Strength")
        from: 0
        to: 6000
        stepSize: 10
        decimals: 0
        value: itemId.scene ? itemId.scene.edl.strength : 0
        onMoved: (newValue) => { if (itemId.scene) itemId.scene.edl.strength = newValue }
    }

    LabeledSlider {
        objectName: "edlMaxDarkenSlider"
        label: qsTr("Max darken")
        from: 0
        to: 30
        stepSize: 0.25
        decimals: 2
        value: itemId.scene ? itemId.scene.edl.maxDarken : 0
        onMoved: (newValue) => { if (itemId.scene) itemId.scene.edl.maxDarken = newValue }
    }

    LabeledSlider {
        objectName: "edlRadiusSlider"
        label: qsTr("Radius (px)")
        from: 0.5
        to: 8
        stepSize: 0.1
        decimals: 1
        value: itemId.scene ? itemId.scene.edl.radius : 0
        onMoved: (newValue) => { if (itemId.scene) itemId.scene.edl.radius = newValue }
    }
}
