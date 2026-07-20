import QtQuick as QQ
import QtQuick.Controls as QC
// The app runs the Fusion style (main.cpp). The segment panels
// re-create Fusion's ButtonPanel from the style's color helpers
// instead of reusing it directly: the stock panel draws its inner
// contrast line on all four edges, and in dark mode that light line
// on both sides of the shared border reads as a gap between the
// segments. These panels push the inner line past the seam edge and
// clip it away, so the highlight runs continuously across both
// segments and only the single shared outline divides them.
import QtQuick.Controls.Fusion as FusionStyle
import cavewherelib

// A primary action button and a chevron button joined into one
// GitHub-style split button: a single outline, one shared 1px
// divider, rounded only on the outer corners. The primary button
// always fires clicked() directly - the menu never sits between the
// user and the main action. The chevron only appears when a menu is
// set and carries a tooltip naming what's behind it.
QQ.Row {
    id: splitButtonId

    property QC.Menu menu: null
    property string menuToolTip: qsTr("More options")

    property alias buttonObjectName: mainButtonId.objectName
    property alias text: mainButtonId.text
    property alias iconSource: mainButtonId.icon.source

    signal clicked()

    // Overlap the two 1px borders into a single shared divider.
    spacing: -1

    component SegmentPanel: QQ.Rectangle {
        id: panelId

        required property QC.Button control
        // Which edge joins the other segment: that corner pair is
        // squared off and the inner contrast line skips that edge.
        property bool seamOnLeft: false
        property bool seamOnRight: false

        // Clips the inner line where it overshoots the seam edge.
        clip: true

        radius: 2
        topLeftRadius: seamOnLeft ? 0 : radius
        bottomLeftRadius: seamOnLeft ? 0 : radius
        topRightRadius: seamOnRight ? 0 : radius
        bottomRightRadius: seamOnRight ? 0 : radius

        color: FusionStyle.Fusion.buttonColor(
                   panelId.control.palette, false, panelId.control.down,
                   panelId.control.enabled && panelId.control.hovered)
        gradient: panelId.control.down ? null : faceGradientId
        border.color: FusionStyle.Fusion.buttonOutline(
                          panelId.control.palette, false, panelId.control.enabled)

        QQ.Gradient {
            id: faceGradientId
            QQ.GradientStop {
                position: 0
                color: FusionStyle.Fusion.gradientStart(
                           FusionStyle.Fusion.buttonColor(
                               panelId.control.palette, false, panelId.control.down,
                               panelId.control.enabled && panelId.control.hovered))
            }
            QQ.GradientStop {
                position: 1
                color: FusionStyle.Fusion.gradientStop(
                           FusionStyle.Fusion.buttonColor(
                               panelId.control.palette, false, panelId.control.down,
                               panelId.control.enabled && panelId.control.hovered))
            }
        }

        QQ.Rectangle {
            x: panelId.seamOnLeft ? -2 : 1
            y: 1
            width: parent.width
                   + (panelId.seamOnLeft || panelId.seamOnRight ? 1 : -2)
            height: parent.height - 2
            radius: 2
            border.color: FusionStyle.Fusion.innerContrastLine
            color: "transparent"
        }

        // Repaint the seam column so the divider reads like the rest
        // of the outline. On a light face the raw Fusion outline is
        // what the eye sees as the button edge. On a dark face the
        // near-black outline vanishes against the window and the
        // visible "outline" is actually the inner contrast highlight
        // composited over the face - so the seam paints that same
        // composite. Branch on the face itself, not the app theme, so
        // the divider follows whatever palette the button renders with.
        QQ.Rectangle {
            readonly property QQ.color faceColor:
                FusionStyle.Fusion.buttonColor(panelId.control.palette,
                                               false, false, false)
            readonly property bool darkFace: faceColor.hslLightness < 0.5

            visible: panelId.seamOnLeft || panelId.seamOnRight
            x: panelId.seamOnLeft ? 0 : parent.width - 1
            y: 1
            width: 1
            height: parent.height - 2
            color: darkFace
                   ? Qt.tint(faceColor, FusionStyle.Fusion.innerContrastLine)
                   : FusionStyle.Fusion.buttonOutline(panelId.control.palette,
                                                      false, panelId.control.enabled)
        }
    }

    QC.Button {
        id: mainButtonId

        icon.width: Theme.iconSizeButton
        icon.height: Theme.iconSizeButton

        background: SegmentPanel {
            implicitWidth: 80
            implicitHeight: 24
            control: mainButtonId
            seamOnRight: splitButtonId.menu !== null
        }

        onClicked: {
            splitButtonId.clicked()
        }
    }

    QC.Button {
        id: menuButtonId
        objectName: "menuButton"

        visible: splitButtonId.menu !== null
        height: mainButtonId.height

        icon.source: "qrc:/twbs-icons/icons/chevron-down.svg"
        icon.width: Theme.iconSizeButton
        icon.height: Theme.iconSizeButton

        background: SegmentPanel {
            implicitWidth: 24
            implicitHeight: 24
            control: menuButtonId
            seamOnLeft: true
        }

        QC.ToolTip.visible: hovered && splitButtonId.menuToolTip.length > 0
        QC.ToolTip.text: splitButtonId.menuToolTip
        QC.ToolTip.delay: 500

        onClicked: {
            splitButtonId.menu.popup(menuButtonId, 0, menuButtonId.height)
        }
    }
}
