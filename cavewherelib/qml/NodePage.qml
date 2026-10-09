/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// pragma ComponentBehavior: Bound

import QtQuick as QQ
import cavewherelib
import QtQml
import QtQuick.Layouts
import QtQuick.Controls as QC

// The page of one survey node — a Cave, a Folder, or a Section. The header
// names the node and its kind, the stats sum everything at or below it, and
// the survey tree rooted at the node lists its child nodes and then its trips.
StandardPage {
    id: nodePageArea
    objectName: "cavePage"

    property SurveyNode currentNode

    // Shared down the node pages so every trip page shows in one TripPage item.
    property QQ.Component tripComponent: tripPageComponent

    // cwPageView keeps one item per component and sets each ancestor's item on
    // the way to a page, so each depth needs its own component. A type cannot
    // name itself in its own body, hence the run-time creation.
    readonly property QQ.Component childNodePageComponent:
        Qt.createComponent("cavewherelib", "NodePage", QQ.Component.PreferSynchronous, nodePageArea)

    function tripPageName(trip) {
        return "Trip=" + trip.name;
    }

    //Must match cwLinkGenerator::nodeLink, which builds the same address.
    function nodePageName(node) {
        return "Node=" + node.name;
    }

    function addTrip() {
        nodePageArea.currentNode.addTrip()
        return nodePageArea.currentNode.trip(nodePageArea.currentNode.tripCount - 1)
    }

    function addTripAndNavigate() {
        const lastTrip = nodePageArea.addTrip()
        RootData.pageSelectionModel.gotoPageByName(nodePageArea.PageView.page,
                                                   nodePageArea.tripPageName(lastTrip));
        return lastTrip;
    }

    // The Add button's own action, which follows position the way the menu
    // beside it does: where caves go it adds a cave and opens its page,
    // inside a cave it adds a trip and opens the trip page.
    function addByPosition() {
        if (nodePageArea.takesCaves) {
            nodePageArea.showAdded(RootData.region.addNode(nodePageArea.currentNode, SurveyNodeKind.Cave),
                                   AddVerbsMenu.AddCave)
        } else {
            nodePageArea.addTripAndNavigate()
        }
    }

    // Lands the user on what an Add verb just made: Add Folder and Add Section
    // stay on this page with the new row's name ready to type; Add Cave and
    // Add Trip open the new page.
    function showAdded(object: QQ.QtObject, verb: int) {
        if (verb === AddVerbsMenu.AddFolder || verb === AddVerbsMenu.AddSection) {
            nodePageArea.pendingReveal = object as SurveyNode
            nodePageArea.revealPending()
        } else {
            tripTreeId.openObject(object)
        }
    }

    // Add Trip → Add trip from survey file…: create the trip first
    // (the dialog targets an existing trip), but don't navigate - the
    // dialog's outcome decides. attached()/imported() name the trip
    // after the picked file and navigate; dismissed() deletes the
    // orphan, since the user asked for a trip from a file, not an
    // empty native one.
    function addTripFromSurveyFileWithDialog() {
        addSurveyFileDialogId.trip = nodePageArea.addTrip()
        addSurveyFileDialogId.open()
    }

    // Master §8.7: the trip name comes from the picked file (directory
    // and extension stripped). uniqueTripName() dedupes - setName
    // silently rejects collisions.
    function nameTripFromFileAndNavigate(newTrip, fileName) {
        let slashIndex = fileName.lastIndexOf("/")
        let baseName = slashIndex >= 0 ? fileName.substring(slashIndex + 1) : fileName
        let dotIndex = baseName.lastIndexOf(".")
        if (dotIndex > 0) {
            baseName = baseName.substring(0, dotIndex)
        }
        newTrip.name = nodePageArea.currentNode.uniqueTripName(baseName)
        RootData.pageSelectionModel.gotoPageByName(nodePageArea.PageView.page,
                                                   nodePageArea.tripPageName(newTrip))
    }

    // The first child of an empty node brings the tree back, and the tree
    // draws the new row only once it has been laid out at its new height, so
    // the reveal is retried until the row exists.
    function revealPending() {
        if (nodePageArea.pendingReveal === null) {
            return
        }
        tripTreeId.revealAdded(nodePageArea.pendingReveal)
        if (tripTreeId.currentRowItem() !== null) {
            nodePageArea.pendingReveal = null
        }
    }

    function registerSubPages() {
        if(currentNode) {
            var oldCarpetPage = PageView.page.childPage("Leads")
            if(oldCarpetPage !== RootData.pageSelectionModel.currentPage) {
                if(oldCarpetPage !== null) {
                    RootData.pageSelectionModel.unregisterPage(oldCarpetPage)
                }

                if(PageView.page.name !== "Leads") {
                    RootData.pageSelectionModel.registerPage(PageView.page,
                                                             "Leads",
                                                             caveLeadsPage,
                                                             {"cave":currentNode});
                }
            }

            var oldFixStationsPage = PageView.page.childPage("Fix Stations")
            if(oldFixStationsPage !== RootData.pageSelectionModel.currentPage) {
                if(oldFixStationsPage !== null) {
                    RootData.pageSelectionModel.unregisterPage(oldFixStationsPage)
                }

                if(PageView.page.name !== "Fix Stations") {
                    RootData.pageSelectionModel.registerPage(PageView.page,
                                                             "Fix Stations",
                                                             fixStationsSubPage,
                                                             {"cave":currentNode});
                }
            }
        }
    }

    PageView.onPageChanged: registerSubPages()

    onCurrentNodeChanged: {
        nodePageArea.pendingReveal = null
        instantiatorId.model = nodePageArea.currentNode
        registerSubPages()
    }

    QQ.Component {
        id: caveLeadsPage
        CaveLeadPage {
            anchors.fill: parent

        }
    }

    QQ.Component {
        id: fixStationsSubPage
        FixStationPage {
            anchors.fill: parent
        }
    }

    readonly property bool isNarrow: width < Theme.breakpointPanelCollapse

    // A Folder or Section an Add verb made, still waiting for its tree row.
    property QQ.QtObject pendingReveal: null

    // The narrow layout's Add Trip bar lives inside narrowColumnComponent,
    // whose ids aren't reachable from out here; it publishes itself on
    // completion so the one hint can point at whichever bar is showing.
    property QQ.Item narrowAddTripBar: null

    // Where the node sits decides its Add verbs: where caves go, Add Cave and
    // Add Folder; inside a cave, Add Trip and Add Section.
    readonly property bool takesCaves: nodePageArea.currentNode !== null
                                       && nodePageArea.currentNode.takesCaves

    // A node copied from a survey file: its name and kind belong to the file.
    readonly property bool isSourced: nodePageArea.currentNode !== null
                                      && nodePageArea.currentNode.isSourced

    // tripCount and childNodeCount rather than rowCount(): rowCount() is a
    // plain function, so a binding on it would never re-evaluate when a trip
    // arrives, while the counts announce themselves.
    readonly property bool isEmpty: nodePageArea.currentNode !== null
                                    && nodePageArea.currentNode.tripCount === 0
                                    && nodePageArea.currentNode.childNodeCount === 0

    // The tree shows the node's child nodes and trips, so it stays away while
    // the page stands for no node: a null rootNode is a whole-region tree,
    // which is the Data page's reading of it and no node page's.
    readonly property bool showsTripTree: nodePageArea.currentNode !== null
                                          && !nodePageArea.isEmpty

    // True while this cave's centerline comes from an attached survey
    // file. The trips such a cave holds are windows into that file, so
    // the page shows the attachment's own state and stops inviting
    // native trips.
    readonly property bool caveAttached: nodePageArea.currentNode !== null
                                         && nodePageArea.currentNode.externalCenterline.entryFile.length > 0

    // Whether the warnings banner has anything to list. Drives the banner
    // proxies' visibility directly — the banner's own visibility is controlled
    // by whichever proxy hosts it, so this reads the banner's count and never
    // its visible (that clobbers the proxy's imperative control and deadlocks).
    readonly property bool hasWarnings: warningsBannerId.count > 0

    // The date of the newest trip at or below the node, as the tree's Date
    // cell writes a date; a dash while no trip there has one.
    readonly property string lastSurveyText: isNaN(nodePageModelId.lastSurvey.getTime())
                                             ? "—"
                                             : Qt.formatDate(nodePageModelId.lastSurvey, Qt.ISODate)

    readonly property string addButtonText: nodePageArea.takesCaves ? qsTr("Add Cave") : qsTr("Add Trip")
    readonly property string addMenuToolTip: nodePageArea.takesCaves
                                             ? qsTr("More ways to add here")
                                             : qsTr("More ways to add a trip")

    // Brings the attached file's source line into view and pulses it. The
    // wide page scrolls as a whole; the narrow column always shows it under
    // the stats.
    function showSourceLine() {
        if (!nodePageArea.isNarrow) {
            const top = caveSummaryId.mapToItem(wideFlickableId.contentItem, 0, 0).y - Theme.pageMargin
            const maxContentY = Math.max(0, wideFlickableId.contentHeight - wideFlickableId.height)
            wideFlickableId.contentY = Math.min(Math.max(0, top), maxContentY)
        }
        caveSummaryId.drawAttention()
    }

    // --- Standalone items (defined once, proxied into wide/narrow layouts) ---

    // The header: the kind's icon, the name, the Kind chip (a Cave/Folder
    // picker on a native node) and Rename. One instance serves both layouts.
    RowLayout {
        id: nodeHeaderId
        spacing: Theme.delegatePadding

        Icon {
            objectName: "nodeKindIcon"
            source: nodePageArea.currentNode !== null && nodePageArea.currentNode.kind === SurveyNodeKind.Folder
                    ? "qrc:/twbs-icons/icons/folder.svg"
                    : "qrc:/icons/svg/caveKind.svg"
            sourceSize: Qt.size(Theme.iconSizeButton, Theme.iconSizeButton)
            colorizationColor: Theme.textSubtle
            visible: nodePageArea.currentNode !== null && !nodePageArea.isSourced
        }

        DoubleClickTextInput {
            id: caveNameText
            objectName: "nodeNameText"
            text: nodePageArea.currentNode ? nodePageArea.currentNode.name : ""
            font.bold: true
            font.pixelSize: Theme.fontSizeTitle
            wrapMode: QQ.Text.WordWrap
            readOnly: nodePageArea.isSourced

            // Wraps once the row runs out of room, and otherwise keeps the
            // chip and Rename right after the name.
            Layout.fillWidth: true
            Layout.maximumWidth: caveNameText.implicitWidth

            onFinishedEditting: (newText) => {
                                    nodePageArea.currentNode.name = newText
                                }
        }

        KindChip {
            objectName: "nodeKindChip"
            text: nodePageModelId.kindLabel
            sourced: nodePageArea.isSourced
            node: nodePageArea.currentNode
        }

        QC.Button {
            objectName: "renameNodeButton"
            text: qsTr("Rename…")
            flat: true
            visible: nodePageArea.currentNode !== null && !nodePageArea.isSourced

            onClicked: caveNameText.openEditor()
        }

        QQ.Item { Layout.fillWidth: true }
    }

    // What the stats row needs beyond the node's own properties.
    NodePageModel {
        id: nodePageModelId
        node: nodePageArea.currentNode
    }

    RowLayout {
        id: tripsStatRow
        spacing: Theme.delegatePadding

        QC.Label {
            text: qsTr("Trips:")
        }

        QC.Label {
            objectName: "tripCountValue"
            text: nodePageModelId.tripCount
        }
    }

    RowLayout {
        id: lastSurveyStatRow
        spacing: Theme.delegatePadding

        QC.Label {
            text: qsTr("Last survey:")
        }

        QC.Label {
            objectName: "lastSurveyValue"
            text: nodePageArea.lastSurveyText
        }
    }

    NodeWarningsBanner {
        id: warningsBannerId

        node: nodePageArea.currentNode

        onSourceLineRequested: nodePageArea.showSourceLine()
    }

    SelectableCaveStat {
        id: lengthStat
        label: "Length:"
        unitValue: nodePageArea.currentNode ? nodePageArea.currentNode.length : null
    }

    SelectableCaveStat {
        id: depthStat
        label: "Depth:"
        unitValue: nodePageArea.currentNode ? nodePageArea.currentNode.depth : null
        depth: true
    }

    RowLayout {
        id: leadsRow
        spacing: Theme.delegatePadding

        QC.Label {
            text: "Leads:"
        }

        LinkText {
            objectName: "leadsLink"
            text: leadModelId.count
            onClicked: {
                RootData.pageSelectionModel.gotoPageByName(nodePageArea.PageView.page, "Leads");
            }
        }
    }

    RowLayout {
        id: fixStationsRow
        spacing: Theme.delegatePadding

        QC.Label {
            text: "Fix stations:"
        }

        LinkText {
            objectName: "fixStationsLink"
            text: nodePageArea.currentNode ? nodePageArea.currentNode.fixStationCount : 0
            onClicked: {
                RootData.pageSelectionModel.gotoPageByName(nodePageArea.PageView.page, "Fix Stations");
            }
        }

        FixStationErrorBadge {
            objectName: "fixStationsBadge"
            errorModel: nodePageArea.currentNode ? nodePageArea.currentNode.errorModel : null
            errorTypeIds: RootData.region.fixStationValidator.fixStationErrorTypeIds
        }
    }

    ColumnLayout {
        id: gridConvergenceCell
        spacing: Theme.tightSpacing

        RowLayout {
            spacing: Theme.delegatePadding

            LabelWithHelp {
                text: "Grid convergence:"
                helpArea: gridConvergenceHelpArea
            }

            QC.Label {
                objectName: "gridConvergenceValue"
                text: nodePageArea.currentNode ? nodePageArea.currentNode.gridConvergence.text : ""

                QQ.HoverHandler {
                    id: gridConvergenceHoverId
                }

                // The n/a readings have nothing the label doesn't already say —
                // detailText repeats it verbatim — so only a real angle earns a
                // tooltip.
                QC.ToolTip.visible: gridConvergenceHoverId.hovered
                                    && nodePageArea.currentNode
                                    && nodePageArea.currentNode.gridConvergence.state === GridConvergence.Valid
                QC.ToolTip.text: nodePageArea.currentNode ? nodePageArea.currentNode.gridConvergence.detailText : ""
            }
        }

        HelpArea {
            id: gridConvergenceHelpArea
            objectName: "gridConvergenceHelp"
            Layout.fillWidth: true
            text: "<p><b>Grid convergence</b> is the angle between <i>true north</i> " +
                  "(the direction to the geographic pole) and <i>grid north</i> " +
                  "(the y-axis of the projected coordinate system).</p>" +
                  "<p>The projection here is the one CaveWhere derived for the " +
                  "project, centered on the first thing you georeferenced — so " +
                  "convergence is zero right there and grows as a cave sits farther " +
                  "east or west of it.</p>" +
                  "<p>When CaveWhere computes 3D positions via survex/cavern, the " +
                  "bearing correction applied to each compass reading is " +
                  "<b>(magnetic declination − grid convergence)</b>, so corrected " +
                  "bearings end up aligned to grid north (the projection's y-axis), " +
                  "not true north. This readout shows the convergence value being " +
                  "used at this cave's anchor.</p>"
        }
    }

    ExternalCenterlineCaveSummary {
        id: caveSummaryId
        Layout.fillWidth: true
        cave: nodePageArea.currentNode as Cave
    }

    // The node's contents, in the same tree the Data page shows, rooted at
    // this node: its child nodes (Sections, or caves in a Folder) come first,
    // then its trips, each row with the tree's own context menu. One instance
    // serves both layouts through the proxies below, so the narrow page shows
    // the tree the wide page shows rather than a list of its own.
    SurveyTreeView {
        id: tripTreeId
        objectName: "tripTree"

        removeAskBox: removeChallengeId
        rootNode: nodePageArea.currentNode
        // The wide page scrolls as a whole, so the tree gives up its own
        // scrolling there and takes the height every row needs. The narrow
        // page hands it what is left of the window and lets it scroll.
        scrollable: nodePageArea.isNarrow

        // Set here rather than on the proxies: a LayoutItemProxy forwards its
        // target's Layout properties, so one set of them serves both layouts.
        Layout.fillWidth: true
        Layout.fillHeight: nodePageArea.isNarrow
        Layout.preferredHeight: nodePageArea.isNarrow ? -1 : tripTreeId.fullHeight
    }

    QQ.Flow {
        id: actionBar
        spacing: Theme.actionBarSpacing
        Layout.fillWidth: true

        AddAndSearchBar {
            id: addTripBarWideId
            objectName: "addTrip"
            addButtonText: nodePageArea.addButtonText
            menu: addTripMenuId
            menuToolTip: nodePageArea.addMenuToolTip
            onAdd: nodePageArea.addByPosition()
        }

        ExportImportButtons {
            id: exportButton
            objectName: "exportImportButtons"
            visible: RootData.desktopBuild && nodePageModelId.tripCount > 0
            currentRegion: RootData.region
            currentCave: nodePageArea.currentNode as Cave
            // The tree's current row, which the export verbs take a trip
            // from. A cave row leaves them without one.
            currentTrip: tripTreeId.currentObject as Trip
        }
    }

    // Sits outside both layouts and positions itself, so one hint serves
    // wide and narrow. The arrow points at the Add button, which is why the
    // text doesn't name the button.
    HelpQuoteBox {
        id: noTripsHintId
        objectName: "noTripsHint"

        readonly property QQ.Item targetBar: nodePageArea.isNarrow
                                             ? nodePageArea.narrowAddTripBar
                                             : addTripBarWideId

        z: 10
        triangleOffset: 0.0
        // The hint is a sibling of the scrolling area it points into, so
        // it has to retire itself when the bar scrolls out of view.
        visibilityClip: nodePageArea
        // Names the control rather than drawing a glyph: the button shows a
        // stroked chevron, not the solid triangle a "▾" renders, and a bare
        // symbol inside qsTr has no font-coverage or translator guarantee.
        text: nodePageArea.takesCaves
              ? qsTr("Nothing here yet — add a cave here, or use the menu beside this button to add a folder.")
              : qsTr("No trips yet — add one here, or use the menu beside this button to add one from a survey file.")
        visible: nodePageArea.isEmpty && !nodePageArea.caveAttached
                 && noTripsHintId.targetBar !== null
        pointAtObject: noTripsHintId.targetBar
        pointAtObjectPosition: noTripsHintId.targetBar !== null
                               ? Qt.point(noTripsHintId.targetBar.width / 2.0,
                                          noTripsHintId.targetBar.height)
                               : Qt.point(0, 0)
    }

    // The Add verbs for this node's position, then the survey-file entry,
    // which makes a trip and so belongs only where trips go.
    AddVerbsMenu {
        id: addTripMenuId
        objectName: "addTripMenu"

        node: nodePageArea.currentNode

        onAdded: (object, verb) => nodePageArea.showAdded(object, verb)

        QC.MenuItem {
            objectName: "addExternalTripMenuItem"
            text: qsTr("Add trip from survey file…")
            visible: !nodePageArea.takesCaves
            enabled: !nodePageArea.takesCaves
            height: visible ? implicitHeight : 0
            onTriggered: nodePageArea.addTripFromSurveyFileWithDialog()
        }
    }

    AddSurveyFileDialog {
        id: addSurveyFileDialogId

        onAttached: {
            let newTrip = addSurveyFileDialogId.trip
            if (newTrip === null) {
                return
            }
            nodePageArea.nameTripFromFileAndNavigate(
                newTrip, newTrip.externalCenterline.entryFile)
        }

        onImported: (sourcePath) => {
            let newTrip = addSurveyFileDialogId.trip
            if (newTrip === null) {
                return
            }
            nodePageArea.nameTripFromFileAndNavigate(newTrip, sourcePath)
        }

        onDismissed: {
            let orphanTrip = addSurveyFileDialogId.trip
            addSurveyFileDialogId.trip = null
            if (orphanTrip === null || nodePageArea.currentNode === null) {
                return
            }
            let index = nodePageArea.currentNode.indexOf(orphanTrip)
            if (index >= 0) {
                nodePageArea.currentNode.removeTrip(index)
            }
        }
    }

    QQ.Loader {
        id: narrowLoaderId
        active: nodePageArea.isNarrow
        visible: nodePageArea.isNarrow
        Layout.fillWidth: true
        Layout.fillHeight: true
        sourceComponent: narrowColumnComponent
    }

    // --- Wide layout ---
    // Page-level Flickable so the stats column can grow (e.g. when the
    // grid-convergence HelpArea expands) past the viewport and the user
    // can scroll the entire page. Using Flickable + attached ScrollBar
    // because QC.ScrollView's attached ScrollBar.vertical doesn't
    // auto-anchor under Qt 6.11 macOS — the bar ends up at (0,0).
    QQ.Flickable {
        id: wideFlickableId
        visible: !nodePageArea.isNarrow
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: wideLayoutId.implicitHeight + 2 * Theme.pageMargin
        // Desktop page, not a touch list — no overshoot bounce.
        boundsBehavior: QQ.Flickable.StopAtBounds

        QC.ScrollBar.vertical: QC.ScrollBar {
            objectName: "cavePageVerticalScrollBar"
            policy: QC.ScrollBar.AsNeeded
        }

        RowLayout {
            id: wideLayoutId
            x: Theme.pageMargin
            y: Theme.pageMargin
            width: wideFlickableId.width - 2 * Theme.pageMargin
            spacing: Theme.columnGap

            ColumnLayout {
                Layout.minimumWidth: statsBoxId.implicitWidth
                Layout.maximumWidth: Theme.infoColumnMaxWidth
                Layout.alignment: Qt.AlignTop
                spacing: Theme.flowSpacing

                LayoutItemProxy {
                    target: nodePageArea.isNarrow ? null : nodeHeaderId
                    Layout.fillWidth: true
                }

                LayoutItemProxy {
                    objectName: "nodeWarningsBannerProxy"
                    target: nodePageArea.isNarrow ? null : warningsBannerId
                    // Hide the proxy when there is no warning: a visible proxy
                    // whose target is invisible still forwards the target's
                    // implicit height, reserving an empty full-width slot (an
                    // empty "badge"). An invisible layout item is excluded.
                    visible: nodePageArea.hasWarnings
                    Layout.fillWidth: true
                }

                SectionGroupBox {
                    id: statsBoxId
                    Layout.fillWidth: true

                    ColumnLayout {
                        id: statsColumnId
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: Theme.tightSpacing

                        LayoutItemProxy { target: tripsStatRow }
                        LayoutItemProxy { target: lengthStat }
                        LayoutItemProxy { target: depthStat }
                        LayoutItemProxy { target: lastSurveyStatRow }

                        QQ.Item { implicitHeight: Theme.delegatePadding }

                        LayoutItemProxy { target: fixStationsRow }
                        LayoutItemProxy { target: leadsRow }
                        LayoutItemProxy { target: gridConvergenceCell }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: Theme.sectionSpacing

                LayoutItemProxy { target: actionBar }

                LayoutItemProxy {
                    target: caveSummaryId
                    visible: nodePageArea.caveAttached && !nodePageArea.isNarrow
                }

                // An empty cave has nothing to tabulate, so the tree stays
                // out of the hint's way. Both proxies hidden hides the tree
                // itself, which is how a proxied item is put away.
                LayoutItemProxy {
                    target: tripTreeId
                    visible: !nodePageArea.isNarrow && nodePageArea.showsTripTree
                }
            }
        }
    }

    // --- Narrow layout ---
    // The column itself is narrowColumnComponent, below.
    QQ.Item {
        visible: nodePageArea.isNarrow
        anchors.fill: parent
        anchors.margins: Theme.pageMargin

        LayoutItemProxy { target: narrowLoaderId; anchors.fill: parent }
    }

    LeadModel {
        id: leadModelId
        regionModel: RootData.regionTreeModel
        cave: nodePageArea.currentNode as Cave
    }

    // The narrow column: the same tree the wide page shows, under the cave's
    // stats — what the wide layout puts in two columns, stacked, with the tree
    // scrolling itself rather than riding a page-wide Flickable.
    QQ.Component {
        id: narrowColumnComponent

        ColumnLayout {
            spacing: Theme.sectionSpacing

            LayoutItemProxy {
                target: nodePageArea.isNarrow ? nodeHeaderId : null
                Layout.fillWidth: true
            }

            LayoutItemProxy {
                target: nodePageArea.isNarrow ? warningsBannerId : null
                visible: nodePageArea.hasWarnings
                Layout.fillWidth: true
            }

            QQ.Flow {
                Layout.fillWidth: true
                spacing: Theme.flowSpacing

                RowLayout {
                    spacing: Theme.delegatePadding

                    QC.Label { text: qsTr("Trips:") }
                    QC.Label { text: nodePageModelId.tripCount }
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                SelectableCaveStat {
                    label: "Length:"
                    unitValue: nodePageArea.currentNode ? nodePageArea.currentNode.length : null
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                SelectableCaveStat {
                    label: "Depth:"
                    unitValue: nodePageArea.currentNode ? nodePageArea.currentNode.depth : null
                    depth: true
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                RowLayout {
                    spacing: Theme.delegatePadding

                    QC.Label { text: qsTr("Last survey:") }
                    QC.Label { text: nodePageArea.lastSurveyText }
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                RowLayout {
                    spacing: Theme.delegatePadding

                    QC.Label { text: "Fix stations:" }

                    LinkText {
                        text: nodePageArea.currentNode ? nodePageArea.currentNode.fixStationCount : 0
                        onClicked: {
                            RootData.pageSelectionModel.gotoPageByName(nodePageArea.PageView.page, "Fix Stations");
                        }
                    }

                    FixStationErrorBadge {
                        errorModel: nodePageArea.currentNode ? nodePageArea.currentNode.errorModel : null
                        errorTypeIds: RootData.region.fixStationValidator.fixStationErrorTypeIds
                    }
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                RowLayout {
                    spacing: Theme.delegatePadding

                    QC.Label { text: "Leads:" }

                    LinkText {
                        text: leadModelId.count
                        onClicked: {
                            RootData.pageSelectionModel.gotoPageByName(nodePageArea.PageView.page, "Leads");
                        }
                    }
                }
            }

            AddAndSearchBar {
                id: addTripBarNarrowId
                objectName: "addTrip"
                addButtonText: nodePageArea.addButtonText
                menu: addTripMenuId
                menuToolTip: nodePageArea.addMenuToolTip
                onAdd: nodePageArea.addByPosition()

                QQ.Component.onCompleted: nodePageArea.narrowAddTripBar = addTripBarNarrowId
                QQ.Component.onDestruction: nodePageArea.narrowAddTripBar = null
            }

            LayoutItemProxy {
                target: caveSummaryId
                visible: nodePageArea.caveAttached && nodePageArea.isNarrow
            }

            LayoutItemProxy {
                target: tripTreeId
                visible: nodePageArea.isNarrow && nodePageArea.showsTripTree
            }

            //Holds the tree up when an empty cave leaves it out, so the
            //hint keeps the space it points into.
            QQ.Item {
                Layout.fillHeight: nodePageArea.isEmpty
            }
        }
    }

    QQ.Connections {
        target: tripTreeId
        enabled: nodePageArea.pendingReveal !== null

        function onHeightChanged() {
            Qt.callLater(nodePageArea.revealPending)
        }

        function onVisibleChanged() {
            Qt.callLater(nodePageArea.revealPending)
        }
    }

    // The tree asks through this prompt and removes what it asked about, so
    // the page only has to hand it over.
    RemoveAskBox {
        id: removeChallengeId
    }

    Instantiator {
        id: instantiatorId

        component Delegate: QQ.QtObject {
            id: delegateObjectId
            required property Trip tripObjectRole
            property Page page
        }

        delegate: Delegate {
        }

        onObjectAdded: (index, object) => {
                           //In-ables the link
                           let trip = (object as Delegate).tripObjectRole
                           var page = RootData.pageSelectionModel.registerPage(nodePageArea.PageView.page, //From
                                                                               nodePageArea.tripPageName(trip), //Name
                                                                               nodePageArea.tripComponent, //component
                                                                               {"currentTrip":trip}
                                                                               )
                           object.page = page;
                           page.setNamingFunction(trip, //The trip that's signaling
                                                  "nameChanged()", //Signal
                                                  nodePageArea, //The object that has renaming function
                                                  "tripPageName", //The function that will generate the name
                                                  trip) //The paramaters to tripPageName() function
                       }

        onObjectRemoved: (index, object) => {
                             RootData.pageSelectionModel.unregisterPage((object as Delegate).page);
                         }
    }

    // A child's page is this same page type, so registration repeats to any depth.
    Instantiator {
        id: childNodeInstantiatorId

        component ChildNodeDelegate: QQ.QtObject {
            required property SurveyNode nodeObjectRole
            property Page page
        }

        model: SurveyNodeChildModel {
            node: nodePageArea.currentNode
        }

        delegate: ChildNodeDelegate {
        }

        onObjectAdded: (index, object) => {
                           const delegate = object as ChildNodeDelegate
                           const node = delegate.nodeObjectRole
                           const page = RootData.pageSelectionModel.registerPage(nodePageArea.PageView.page,
                                                                                 nodePageArea.nodePageName(node),
                                                                                 nodePageArea.childNodePageComponent,
                                                                                 {"currentNode": node,
                                                                                  "tripComponent": nodePageArea.tripComponent})
                           delegate.page = page
                           page.setNamingFunction(node, "nameChanged()", nodePageArea, "nodePageName", node)
                       }

        onObjectRemoved: (index, object) => {
                             RootData.pageSelectionModel.unregisterPage((object as ChildNodeDelegate).page)
                         }
    }

    QQ.Component {
        id: tripPageComponent
        TripPage {
            anchors.fill: parent
        }
    }
}
