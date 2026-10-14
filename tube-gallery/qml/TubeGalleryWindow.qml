import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Dialogs as QD
import QtQuick.Layouts
import cavewherelib
import cw.TubeGallery

QC.ApplicationWindow {
    id: rootId

    // Set from the command line by main.cpp.
    property string initialCase: ""
    property int randomSeed: -1
    property int randomJunctions: 3
    property bool randomWild: false
    property string grabDirectory: ""
    property bool grabAll: false
    property bool failingOnly: false
    property bool includeSweep: false
    property bool showRings: false
    property bool showReadings: false
    property bool showSections: false
    property bool showOvershoot: false
    property list<string> projectFiles: []
    property real projectSpacing: 1.0
    property string focusStation: ""
    property string defaultGrabDirectory: ""

    readonly property list<string> presetNames: ["plan", "profile", "along-arm", "three-quarter"]
    readonly property int initialWidth: 1440
    readonly property int initialHeight: 900
    readonly property int defaultSeed: 7
    readonly property int listWidth: 360
    readonly property int panelMargin: 8
    readonly property int panelSpacing: 6
    readonly property int reportHeight: 220
    readonly property int markerSize: 10
    readonly property real ringHoverPixels: 12.0
    readonly property int badgeMinWidth: 28
    readonly property int badgePadding: 6
    readonly property int rowPadding: 6
    readonly property real translucentOpacity: 0.5
    readonly property real threeQuarterAzimuthOffset: 45.0
    readonly property real threeQuarterPitch: 35.0
    readonly property real planPitch: 90.0
    readonly property real profileAzimuthOffset: 90.0
    // The along-arm eye sits this far from J toward the far end, inside the
    // arm, so the dead-end cap stays behind it.
    readonly property real alongArmEyeFraction: 0.9
    readonly property int grabSettleMilliseconds: 300
    readonly property int startupDelayMilliseconds: 200
    readonly property int maxSeed: 1000000
    readonly property int maxJunctions: 50

    // Screenshot queue: every preset of each row in grabRows.
    property list<int> grabRows: []
    property int grabRowCursor: 0
    property int grabPresetCursor: 0
    property string activeGrabDirectory: ""
    property bool quitAfterGrab: false
    property bool grabFailed: false

    // Projects from the command line still loading, and the first row they
    // added.
    property int pendingProjects: 0
    property int firstProjectRow: -1

    // The ring whose line the report shows: the one last hovered, or the one
    // nearest the issue marker last tapped.
    property int inspectedRing: -1

    width: initialWidth
    height: initialHeight
    visible: true
    title: qsTr("Passage Tube Gallery")
    color: Theme.background

    menuBar: QC.MenuBar {
        visible: !rootId.grabAll

        QC.Menu {
            title: qsTr("File")

            QC.MenuItem {
                text: qsTr("Open project…")
                onTriggered: openProjectDialogId.open()
            }
        }
    }

    function degrees(radians: real): real {
        return radians * 180.0 / Math.PI
    }

    function useOrtho(ortho: bool) {
        viewerId.orthoProjection.enabled = ortho
        viewerId.perspectiveProjection.enabled = !ortho
    }

    function applyPreset(preset: string) {
        const box = galleryModelId.framingBox
        const direction = galleryModelId.armDirection
        const armAzimuth = degrees(Math.atan2(direction.x, direction.y))

        if (preset === "plan") {
            useOrtho(true)
            turnTableId.setViewState(turnTableId.framingViewState(box, 0.0, planPitch))
        } else if (preset === "profile") {
            useOrtho(true)
            turnTableId.setViewState(turnTableId.framingViewState(box, armAzimuth + profileAzimuthOffset, 0.0))
        } else if (preset === "along-arm") {
            useOrtho(false)
            const sight = galleryModelId.junction.minus(galleryModelId.armFarEnd)
            const horizontal = Math.hypot(sight.x, sight.y)
            const pitch = degrees(Math.atan2(-sight.z, horizontal))
            const state = turnTableId.framingViewState(box, armAzimuth, pitch)
            state.center = galleryModelId.junction
            state.distance = sight.length() * alongArmEyeFraction
            state.eyeOffset = Qt.vector3d(0, 0, 0)
            turnTableId.setViewState(state)
        } else {
            useOrtho(false)
            turnTableId.setViewState(turnTableId.framingViewState(box, armAzimuth + threeQuarterAzimuthOffset, threeQuarterPitch))
        }
    }

    function inspectNearestRing(position: QQ.vector3d) {
        inspectedRing = galleryModelId.nearestRing(position)
    }

    function selectRow(row: int) {
        galleryModelId.currentIndex = row
        applyPreset("three-quarter")
    }

    function startGrab(rows: list<int>, directory: string, quitAfter: bool) {
        grabRows = rows
        grabRowCursor = 0
        grabPresetCursor = 0
        activeGrabDirectory = directory
        quitAfterGrab = quitAfter
        grabFailed = false
        prepareGrabStep()
    }

    function prepareGrabStep() {
        if (grabRowCursor >= grabRows.length) {
            finishGrab()
            return
        }
        galleryModelId.currentIndex = grabRows[grabRowCursor]
        applyPreset(presetNames[grabPresetCursor])
        grabTimerId.restart()
    }

    function grabStep() {
        const fileName = TubeGalleryGrabber.fileName(galleryModelId.currentName, presetNames[grabPresetCursor])
        const path = TubeGalleryGrabber.grab(viewAreaId, activeGrabDirectory, fileName)
        if (path === "") {
            console.warn("Could not write", fileName, "to", activeGrabDirectory)
            grabFailed = true
        }

        grabPresetCursor++
        if (grabPresetCursor >= presetNames.length) {
            grabPresetCursor = 0
            grabRowCursor++
        }
        prepareGrabStep()
    }

    function finishGrab() {
        grabRows = []
        if (quitAfterGrab) {
            Qt.exit(grabFailed ? 1 : 0)
        }
    }

    function start() {
        if (projectFiles.length > 0) {
            pendingProjects = projectFiles.length
            for (const file of projectFiles) {
                galleryModelId.openProject(file, projectSpacing)
            }
        } else {
            selectInitialRow()
        }
    }

    function projectSettled() {
        pendingProjects--
        if (pendingProjects === 0) {
            selectInitialRow()
        }
    }

    function selectInitialRow() {
        if (randomSeed >= 0) {
            galleryModelId.addRandom(randomSeed, randomJunctions, randomWild)
            selectRow(galleryModelId.count - 1)
        } else if (initialCase !== "") {
            const row = galleryModelId.indexOf(initialCase)
            if (row < 0) {
                console.warn("No case named", initialCase)
                if (grabDirectory !== "") {
                    Qt.exit(1)
                    return
                }
            }
            selectRow(Math.max(row, 0))
        } else {
            selectRow(Math.max(firstProjectRow, 0))
        }

        if (grabAll) {
            const rows = []
            if (failingOnly) {
                for (const row of galleryModelId.failingRows()) {
                    rows.push(row)
                }
            } else {
                for (let row = 0; row < galleryModelId.count; row++) {
                    rows.push(row)
                }
            }
            startGrab(rows, grabDirectory, true)
        } else if (grabDirectory !== "") {
            startGrab([galleryModelId.currentIndex], grabDirectory, true)
        }
    }

    QQ.Component.onCompleted: startupTimerId.start()

    QQ.Timer {
        id: startupTimerId
        interval: rootId.startupDelayMilliseconds
        onTriggered: rootId.start()
    }

    QQ.Timer {
        id: grabTimerId
        interval: rootId.grabSettleMilliseconds
        onTriggered: rootId.grabStep()
    }

    PassageTubeGalleryModel {
        id: galleryModelId
        includeSweep: rootId.includeSweep
        scene: sceneId
        ringsVisible: ringsId.checked
        framesVisible: framesId.checked
        readingsVisible: readingsId.checked
        sectionsVisible: sectionsId.checked
        overshootVisible: overshootId.checked
        station: stationFieldId.text

        onCurrentChanged: rootId.inspectedRing = -1
        onProjectOpened: (firstRow, rowCount) => {
            if (rootId.pendingProjects > 0) {
                if (rootId.firstProjectRow < 0 && rowCount > 0) {
                    rootId.firstProjectRow = firstRow
                }
                rootId.projectSettled()
            } else if (rowCount > 0) {
                rootId.selectRow(firstRow)
            }
        }
        onProjectFailed: (message) => {
            console.warn(message)
            if (rootId.grabDirectory !== "") {
                Qt.exit(1)
            } else if (rootId.pendingProjects > 0) {
                rootId.projectSettled()
            }
        }
    }

    QD.FileDialog {
        id: openProjectDialogId
        title: qsTr("Open project")
        nameFilters: [qsTr("CaveWhere projects (*.cw *.cwproj)")]

        onAccepted: galleryModelId.openProject(openProjectDialogId.selectedFile.toString(), rootId.projectSpacing)
    }

    PassageTubeGalleryFilterModel {
        id: filterModelId
        sourceModel: galleryModelId
        filterText: filterFieldId.text
        failingFirst: failingFirstId.checked
    }

    component IssueMarker: QQ.Rectangle {
        id: issueMarkerId

        required property QQ.vector3d modelData
        required property TransformUpdater transformUpdater

        readonly property point viewPoint: {
            issueMarkerId.transformUpdater.matrix
            return issueMarkerId.transformUpdater.mapModelToViewport(issueMarkerId.modelData)
        }

        signal tapped()

        x: viewPoint.x - width / 2
        y: viewPoint.y - height / 2
        radius: width / 2
        border.color: Theme.textInverse

        QQ.TapHandler {
            onTapped: issueMarkerId.tapped()
        }
    }

    Scene {
        id: sceneId
    }

    TransformUpdater {
        id: transformUpdaterId
        camera: viewerId.camera
    }

    QQ.Binding {
        target: galleryModelId.tube
        property: "wireframe"
        value: wireframeId.checked
    }

    QQ.Binding {
        target: galleryModelId.tube
        property: "opacity"
        value: translucentId.checked ? rootId.translucentOpacity : 1.0
    }

    QQ.Binding {
        target: galleryModelId.tube
        property: "clipEnabled"
        value: cutId.checked
    }

    // Through the junction, normal to the camera's right axis.
    QQ.Binding {
        target: galleryModelId.tube
        property: "clipPlane"
        value: {
            const view = viewerId.camera.viewMatrix
            const right = Qt.vector3d(view.m11, view.m12, view.m13).normalized()
            return Qt.vector4d(right.x, right.y, right.z, -right.dotProduct(galleryModelId.junction))
        }
    }

    QQ.Binding {
        target: galleryModelId.linePlot
        property: "visible"
        value: centerlineId.checked
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: rootId.panelMargin
        spacing: rootId.panelMargin

        ColumnLayout {
            Layout.preferredWidth: rootId.listWidth
            Layout.fillHeight: true
            visible: !rootId.grabAll
            spacing: rootId.panelSpacing

            QC.TextField {
                id: filterFieldId
                Layout.fillWidth: true
                placeholderText: qsTr("Filter by name or tag")
            }

            QC.CheckBox {
                id: failingFirstId
                text: qsTr("Failing first")
            }

            RowLayout {
                Layout.fillWidth: true

                QC.SpinBox {
                    id: seedId
                    from: 0
                    to: rootId.maxSeed
                    value: rootId.defaultSeed
                    editable: true
                }

                QC.SpinBox {
                    id: junctionsId
                    from: 1
                    to: rootId.maxJunctions
                    value: rootId.randomJunctions
                    editable: true
                }

                QC.Button {
                    text: qsTr("Add random")
                    onClicked: {
                        galleryModelId.addRandom(seedId.value, junctionsId.value)
                        rootId.selectRow(galleryModelId.count - 1)
                    }
                }
            }

            QQ.ListView {
                id: caseListId
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: filterModelId

                delegate: QQ.Rectangle {
                    id: caseRowId

                    required property int index
                    required property string name
                    required property string tags
                    required property int issueCount
                    required property string issueSummary

                    width: QQ.ListView.view.width
                    height: rowLayoutId.implicitHeight + 2 * rootId.rowPadding
                    color: filterModelId.sourceRow(caseRowId.index) === galleryModelId.currentIndex ? Theme.highlight : "transparent"

                    QQ.TapHandler {
                        onTapped: rootId.selectRow(filterModelId.sourceRow(caseRowId.index))
                    }

                    RowLayout {
                        id: rowLayoutId
                        anchors.fill: parent
                        anchors.margins: rootId.rowPadding

                        QC.Label {
                            Layout.fillWidth: true
                            text: caseRowId.name
                            elide: QQ.Text.ElideRight
                            font.pixelSize: Theme.fontSizeSmall
                            QC.ToolTip.visible: hoverId.hovered
                            QC.ToolTip.text: caseRowId.issueSummary !== "" ? caseRowId.issueSummary : caseRowId.tags

                            QQ.HoverHandler {
                                id: hoverId
                            }
                        }

                        QQ.Rectangle {
                            Layout.preferredWidth: Math.max(rootId.badgeMinWidth, badgeLabelId.implicitWidth + 2 * rootId.badgePadding)
                            Layout.preferredHeight: badgeLabelId.implicitHeight
                            radius: height / 2
                            color: caseRowId.issueCount === 0 ? Theme.success : Theme.errorText

                            QC.Label {
                                id: badgeLabelId
                                anchors.centerIn: parent
                                text: caseRowId.issueCount
                                color: Theme.textInverse
                                font.pixelSize: Theme.fontSizeCaption
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: rootId.panelSpacing

            QC.Label {
                text: galleryModelId.currentName
                font.pixelSize: Theme.fontSizeMedium
            }

            RowLayout {
                visible: !rootId.grabAll

                QC.Button {
                    text: qsTr("Plan")
                    onClicked: rootId.applyPreset("plan")
                }
                QC.Button {
                    text: qsTr("Profile")
                    onClicked: rootId.applyPreset("profile")
                }
                QC.Button {
                    text: qsTr("Along arm")
                    onClicked: rootId.applyPreset("along-arm")
                }
                QC.Button {
                    text: qsTr("3/4")
                    onClicked: rootId.applyPreset("three-quarter")
                }
                QC.TextField {
                    id: stationFieldId
                    placeholderText: qsTr("Station")
                    text: rootId.focusStation

                    onEditingFinished: rootId.applyPreset("three-quarter")
                }
                QC.CheckBox {
                    id: wireframeId
                    text: qsTr("Wireframe")
                }
                QC.CheckBox {
                    id: cutId
                    text: qsTr("Cut")
                }
                QC.CheckBox {
                    id: translucentId
                    text: qsTr("Translucent")
                }
                QC.CheckBox {
                    id: issuesId
                    text: qsTr("Issues")
                    checked: true
                }
                QC.CheckBox {
                    id: ringsId
                    text: qsTr("Rings")
                    checked: rootId.showRings
                }
                QC.CheckBox {
                    id: framesId
                    text: qsTr("Frames")
                    checked: rootId.showRings
                }
                QC.CheckBox {
                    id: readingsId
                    text: qsTr("Readings")
                    checked: rootId.showReadings
                }
                QC.CheckBox {
                    id: sectionsId
                    text: qsTr("Sections")
                    checked: rootId.showSections
                }
                QC.CheckBox {
                    id: overshootId
                    text: qsTr("Overshoot")
                    checked: rootId.showOvershoot
                }
                QC.CheckBox {
                    id: centerlineId
                    text: qsTr("Centerline")
                    checked: true
                }
                QC.Button {
                    text: qsTr("Grab sheet")
                    enabled: galleryModelId.currentIndex >= 0 && rootId.grabRows.length === 0
                    onClicked: rootId.startGrab([galleryModelId.currentIndex], rootId.defaultGrabDirectory, false)
                }
            }

            QQ.Item {
                id: viewAreaId
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                RegionViewer {
                    id: viewerId
                    anchors.fill: parent
                    scene: sceneId
                }

                TurnTableInteraction {
                    id: turnTableId
                    anchors.fill: parent
                    camera: viewerId.camera
                    scene: sceneId
                }

                QQ.HoverHandler {
                    id: ringHoverId
                    enabled: ringsId.checked || framesId.checked || sectionsId.checked || overshootId.checked

                    onPointChanged: {
                        const ring = galleryModelId.ringAt(ringHoverId.point.position, transformUpdaterId.matrix, rootId.ringHoverPixels)
                        if (ring >= 0) {
                            rootId.inspectedRing = ring
                        }
                    }
                }

                QQ.Repeater {
                    model: issuesId.checked ? galleryModelId.warningPositions : []

                    delegate: IssueMarker {
                        id: warningMarkerId
                        transformUpdater: transformUpdaterId
                        width: rootId.markerSize
                        height: rootId.markerSize
                        color: Theme.warning

                        onTapped: rootId.inspectNearestRing(warningMarkerId.modelData)
                    }
                }

                QQ.Repeater {
                    model: issuesId.checked ? galleryModelId.issuePositions : []

                    delegate: IssueMarker {
                        id: errorMarkerId
                        transformUpdater: transformUpdaterId
                        width: rootId.markerSize
                        height: rootId.markerSize
                        color: Theme.errorText

                        onTapped: rootId.inspectNearestRing(errorMarkerId.modelData)
                    }
                }
            }

            QC.ScrollView {
                id: reportScrollId
                Layout.fillWidth: true
                Layout.preferredHeight: rootId.reportHeight
                visible: !rootId.grabAll

                QC.Label {
                    width: reportScrollId.availableWidth
                    text: (rootId.inspectedRing >= 0 ? galleryModelId.ringLine(rootId.inspectedRing) + "\n\n" : "")
                          + galleryModelId.reportText + "\n\n" + galleryModelId.issueTexts.join("\n")
                          + "\n\n" + galleryModelId.parameters
                    font.family: Theme.fontFamilyMono
                    font.pixelSize: Theme.fontSizeCaption
                    wrapMode: QQ.Text.WrapAnywhere
                }
            }
        }
    }
}
