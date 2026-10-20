import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

// Sliders for the gallery's sandbox junction: the through passage, then one
// group per arm.
QC.ScrollView {
    id: panelId

    required property PassageTubeGalleryModel galleryModel
    property string statusText: ""

    readonly property PassageTubeSandboxModel sandbox: panelId.galleryModel.sandbox
    readonly property list<string> wallNames: [qsTr("Left"), qsTr("Right"), qsTr("Up"), qsTr("Down")]
    readonly property int labelWidth: 96
    readonly property int fieldWidth: 64
    readonly property int groupSpacing: 4
    readonly property int sectionSpacing: 12
    readonly property real angleStep: 1.0
    readonly property real meterStep: 0.1
    readonly property real maxPlanDegrees: 180.0
    readonly property real maxProfileDegrees: 90.0
    readonly property real maxBendDegrees: 180.0
    readonly property real minLengthMeters: 0.5
    readonly property real maxLengthMeters: 30.0
    readonly property real maxWallMeters: 10.0
    readonly property real minSpacingMeters: 0.1
    readonly property real maxSpacingMeters: 4.0

    signal grabRequested()

    contentWidth: availableWidth

    component SliderRow: RowLayout {
        id: sliderRowId

        required property string label
        required property real value
        property real from: 0.0
        property real to: panelId.maxWallMeters
        property real stepSize: panelId.meterStep

        signal edited(real value)

        QC.Label {
            Layout.preferredWidth: panelId.labelWidth
            text: sliderRowId.label
            elide: QQ.Text.ElideRight
            font.pixelSize: Theme.fontSizeSmall
        }

        QC.Slider {
            id: sliderId
            Layout.fillWidth: true
            from: sliderRowId.from
            to: sliderRowId.to
            stepSize: sliderRowId.stepSize
            snapMode: QC.Slider.SnapAlways
            value: sliderRowId.value

            // Rounds through the step count so 0.1 steps give clean decimals.
            onMoved: sliderRowId.edited(Math.round(sliderId.value / sliderRowId.stepSize) / Math.round(1.0 / sliderRowId.stepSize))
        }

        QC.TextField {
            id: fieldId
            Layout.preferredWidth: panelId.fieldWidth
            text: String(sliderRowId.value)
            font.pixelSize: Theme.fontSizeSmall
            validator: QQ.DoubleValidator {
                locale: "C"
            }

            onEditingFinished: sliderRowId.edited(Number(fieldId.text))
        }
    }

    component SectionEditor: ColumnLayout {
        id: sectionEditorId

        required property string title
        required property list<real> walls

        signal wallEdited(int wall, real meters)

        spacing: 0

        QC.Label {
            text: sectionEditorId.title
            font.pixelSize: Theme.fontSizeSmall
            font.bold: true
        }

        QQ.Repeater {
            model: panelId.wallNames

            delegate: SliderRow {
                id: wallRowId

                required property int index
                required property string modelData

                Layout.fillWidth: true
                label: wallRowId.modelData
                value: sectionEditorId.walls[wallRowId.index]

                onEdited: (meters) => sectionEditorId.wallEdited(wallRowId.index, meters)
            }
        }
    }

    ColumnLayout {
        width: panelId.availableWidth
        spacing: panelId.sectionSpacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: panelId.groupSpacing

            QC.Label {
                text: qsTr("Sandbox")
                font.pixelSize: Theme.fontSizeMedium
            }

            BodyText {
                Layout.fillWidth: true
                text: panelId.galleryModel.sandboxBuildText
                font.pixelSize: Theme.fontSizeCaption
                wrapMode: QQ.Text.Wrap
            }

            QQ.Flow {
                Layout.fillWidth: true
                spacing: panelId.groupSpacing

                QC.Button {
                    text: qsTr("Add arm")
                    onClicked: panelId.sandbox.addArm()
                }
                QC.Button {
                    text: qsTr("Remove")
                    enabled: panelId.sandbox.selectedArm >= 0
                    onClicked: panelId.sandbox.removeArm(panelId.sandbox.selectedArm)
                }
                QC.Button {
                    text: qsTr("Copy as case")
                    onClicked: {
                        panelId.sandbox.copyAsCase()
                        panelId.statusText = qsTr("Copied the case to the clipboard")
                    }
                }
                QC.Button {
                    text: qsTr("Grab")
                    onClicked: panelId.grabRequested()
                }
            }

            BodyText {
                Layout.fillWidth: true
                visible: panelId.statusText !== ""
                text: panelId.statusText
                font.pixelSize: Theme.fontSizeCaption
                wrapMode: QQ.Text.WrapAnywhere
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: panelId.groupSpacing

            QC.Label {
                text: qsTr("Through t0 → J → t1")
                font.bold: true
            }
            SliderRow {
                Layout.fillWidth: true
                label: qsTr("Bend")
                to: panelId.maxBendDegrees
                stepSize: panelId.angleStep
                value: panelId.sandbox.throughBendDegrees
                onEdited: (degrees) => panelId.sandbox.throughBendDegrees = degrees
            }
            SliderRow {
                Layout.fillWidth: true
                label: qsTr("Profile")
                from: -panelId.maxProfileDegrees
                to: panelId.maxProfileDegrees
                stepSize: panelId.angleStep
                value: panelId.sandbox.throughProfileDegrees
                onEdited: (degrees) => panelId.sandbox.throughProfileDegrees = degrees
            }
            SliderRow {
                Layout.fillWidth: true
                label: qsTr("Bend profile")
                from: -panelId.maxProfileDegrees
                to: panelId.maxProfileDegrees
                stepSize: panelId.angleStep
                value: panelId.sandbox.throughBendProfileDegrees
                onEdited: (degrees) => panelId.sandbox.throughBendProfileDegrees = degrees
            }
            SliderRow {
                Layout.fillWidth: true
                label: qsTr("Sample spacing")
                from: panelId.minSpacingMeters
                to: panelId.maxSpacingMeters
                value: panelId.sandbox.sampleSpacing
                onEdited: (meters) => panelId.sandbox.sampleSpacing = meters
            }
            SectionEditor {
                Layout.fillWidth: true
                title: qsTr("t0")
                walls: panelId.sandbox.throughStart
                onWallEdited: (wall, meters) => panelId.sandbox.setWall(-1, PassageTubeSandboxModel.ThroughStart, wall, meters)
            }
            SectionEditor {
                Layout.fillWidth: true
                title: qsTr("J, on the through")
                walls: panelId.sandbox.throughJunction
                onWallEdited: (wall, meters) => panelId.sandbox.setWall(-1, PassageTubeSandboxModel.ThroughJunction, wall, meters)
            }
            SectionEditor {
                Layout.fillWidth: true
                title: qsTr("t1")
                walls: panelId.sandbox.throughEnd
                onWallEdited: (wall, meters) => panelId.sandbox.setWall(-1, PassageTubeSandboxModel.ThroughEnd, wall, meters)
            }
        }

        QQ.Repeater {
            model: panelId.sandbox

            delegate: ColumnLayout {
                id: armId

                required property int index
                required property string name
                required property real planDegrees
                required property real profileDegrees
                required property real lengthMeters
                required property bool hasLrud
                required property bool hasJunctionLrud
                required property list<real> farSection
                required property list<real> junctionSection
                required property bool junctionConflict

                Layout.fillWidth: true
                spacing: panelId.groupSpacing

                QC.RadioButton {
                    text: qsTr("Arm J → %1").arg(armId.name)
                    font.bold: true
                    checked: panelId.sandbox.selectedArm === armId.index
                    onClicked: panelId.sandbox.selectedArm = armId.index
                }
                SliderRow {
                    Layout.fillWidth: true
                    label: qsTr("Plan")
                    from: -panelId.maxPlanDegrees
                    to: panelId.maxPlanDegrees
                    stepSize: panelId.angleStep
                    value: armId.planDegrees
                    onEdited: (degrees) => panelId.sandbox.setPlanDegrees(armId.index, degrees)
                }
                SliderRow {
                    Layout.fillWidth: true
                    label: qsTr("Profile")
                    from: -panelId.maxProfileDegrees
                    to: panelId.maxProfileDegrees
                    stepSize: panelId.angleStep
                    value: armId.profileDegrees
                    onEdited: (degrees) => panelId.sandbox.setProfileDegrees(armId.index, degrees)
                }
                SliderRow {
                    Layout.fillWidth: true
                    label: qsTr("Length")
                    from: panelId.minLengthMeters
                    to: panelId.maxLengthMeters
                    value: armId.lengthMeters
                    onEdited: (meters) => panelId.sandbox.setLengthMeters(armId.index, meters)
                }
                RowLayout {
                    QC.CheckBox {
                        text: qsTr("Readings")
                        checked: armId.hasLrud
                        onToggled: panelId.sandbox.setHasLrud(armId.index, checked)
                    }
                    QC.CheckBox {
                        text: qsTr("Reading at J")
                        checked: armId.hasJunctionLrud
                        onToggled: panelId.sandbox.setHasJunctionLrud(armId.index, checked)
                    }
                }
                SectionEditor {
                    Layout.fillWidth: true
                    title: armId.name
                    walls: armId.farSection
                    onWallEdited: (wall, meters) => panelId.sandbox.setWall(armId.index, PassageTubeSandboxModel.ArmFar, wall, meters)
                }
                SectionEditor {
                    Layout.fillWidth: true
                    title: qsTr("J, on this arm")
                    walls: armId.junctionSection
                    onWallEdited: (wall, meters) => panelId.sandbox.setWall(armId.index, PassageTubeSandboxModel.ArmJunction, wall, meters)
                }
                BodyText {
                    Layout.fillWidth: true
                    visible: armId.junctionConflict
                    text: qsTr("J reads %1 on this arm and %2 on the through: two records of one station")
                          .arg(armId.junctionSection.join("/"))
                          .arg(panelId.sandbox.throughJunction.join("/"))
                    color: Theme.warningText
                    font.pixelSize: Theme.fontSizeCaption
                    wrapMode: QQ.Text.Wrap
                }
            }
        }
    }
}
