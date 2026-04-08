import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

ColumnLayout {
    id: itemId

    required property HeadCoupledPerspectiveProjection headProjection
    property AbstractHeadTracker headTracker

    QC.CheckBox {
        text: "Enable"
        checked: itemId.headProjection.enabled
        onToggled: itemId.headProjection.enabled = checked
    }

    QC.Label {
        text: "Smoothing: " + smoothingSliderId.value.toFixed(2)
        font.pixelSize: Theme.fontSizeBody
        visible: itemId.headProjection.enabled
    }
    QC.Slider {
        id: smoothingSliderId
        from: 0.0; to: 1.0
        value: itemId.headTracker ? itemId.headTracker.smoothing : 0.5
        onMoved: if (itemId.headTracker) itemId.headTracker.smoothing = value
        visible: itemId.headProjection.enabled
        Layout.fillWidth: true
    }

    QC.Label {
        text: "Sensitivity: " + sensitivitySliderId.value.toFixed(2)
        font.pixelSize: Theme.fontSizeBody
        visible: itemId.headProjection.enabled
    }
    QC.Slider {
        id: sensitivitySliderId
        from: 0.1; to: 3.0
        value: itemId.headProjection.sensitivity
        onMoved: itemId.headProjection.sensitivity = value
        visible: itemId.headProjection.enabled
        Layout.fillWidth: true
    }

    QC.Label {
        text: "Screen Width (m): " + screenWidthSliderId.value.toFixed(3)
        font.pixelSize: Theme.fontSizeBody
        visible: itemId.headProjection.enabled
    }
    QC.Slider {
        id: screenWidthSliderId
        from: 0.05; to: 1.0
        value: itemId.headProjection.screenWidthMeters
        onMoved: itemId.headProjection.screenWidthMeters = value
        visible: itemId.headProjection.enabled
        Layout.fillWidth: true
    }

    QC.Label {
        text: "Screen Height (m): " + screenHeightSliderId.value.toFixed(3)
        font.pixelSize: Theme.fontSizeBody
        visible: itemId.headProjection.enabled
    }
    QC.Slider {
        id: screenHeightSliderId
        from: 0.05; to: 1.0
        value: itemId.headProjection.screenHeightMeters
        onMoved: itemId.headProjection.screenHeightMeters = value
        visible: itemId.headProjection.enabled
        Layout.fillWidth: true
    }

    QC.Label {
        text: "Parallax Strength: " + parallaxStrengthSliderId.value.toFixed(1)
        font.pixelSize: Theme.fontSizeBody
        visible: itemId.headProjection.enabled
    }
    QC.Slider {
        id: parallaxStrengthSliderId
        from: 0.0; to: 2.0
        value: itemId.headProjection.parallaxStrength
        onMoved: itemId.headProjection.parallaxStrength = value
        visible: itemId.headProjection.enabled
        Layout.fillWidth: true
    }

    QC.CheckBox {
        text: "View Matrix Offset"
        checked: itemId.headProjection.viewMatrixOffsetEnabled
        onToggled: itemId.headProjection.viewMatrixOffsetEnabled = checked
        visible: itemId.headProjection.enabled
    }
}
