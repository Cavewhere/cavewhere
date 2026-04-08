import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib

ColumnLayout {
    id: itemId

    required property HeadCoupledPerspectiveProjection headProjection
    required property AbstractHeadTracker headTracker

    QC.CheckBox {
        text: "Enable"
        checked: itemId.headProjection.enabled
        onToggled: itemId.headProjection.enabled = checked
    }

    QC.Label {
        text: "Smoothing"
        font.pixelSize: Theme.fontSizeBody
        visible: itemId.headProjection.enabled
    }
    QC.Slider {
        from: 0.0; to: 1.0
        value: itemId.headTracker.smoothing
        onMoved: itemId.headTracker.smoothing = value
        visible: itemId.headProjection.enabled
        Layout.fillWidth: true
    }

    QC.Label {
        text: "Sensitivity"
        font.pixelSize: Theme.fontSizeBody
        visible: itemId.headProjection.enabled
    }
    QC.Slider {
        from: 0.1; to: 3.0
        value: itemId.headProjection.sensitivity
        onMoved: itemId.headProjection.sensitivity = value
        visible: itemId.headProjection.enabled
        Layout.fillWidth: true
    }
}
