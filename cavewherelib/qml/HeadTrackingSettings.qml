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
        value: itemId.headTracker ? itemId.headTracker.smoothing : 0.7
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

    QC.Label {
        text: "Translation Scale: " + translationScaleSliderId.value.toFixed(2)
        font.pixelSize: Theme.fontSizeBody
        visible: itemId.headProjection.enabled
    }
    QC.Slider {
        id: translationScaleSliderId
        from: 0.0; to: 3.0
        value: itemId.headProjection.translationScale
        onMoved: itemId.headProjection.translationScale = value
        visible: itemId.headProjection.enabled
        Layout.fillWidth: true
    }

    QC.CheckBox {
        text: "View Matrix Offset"
        checked: itemId.headProjection.viewMatrixOffsetEnabled
        onToggled: itemId.headProjection.viewMatrixOffsetEnabled = checked
        visible: itemId.headProjection.enabled
    }

    QC.CheckBox {
        text: "Debug Preview"
        checked: itemId.headTracker && itemId.headTracker.debugPreview ? true : false
        onToggled: if (itemId.headTracker) itemId.headTracker.debugPreview = checked
        visible: itemId.headProjection.enabled && itemId.headTracker
                 && itemId.headTracker.debugPreview !== undefined
    }
}
