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

StandardPage {
    id: cavePageArea
    objectName: "cavePage"

    property Cave currentCave

    function tripPageName(trip) {
        return "Trip=" + trip.name;
    }

    function addTripAndNavigate() {
        cavePageArea.currentCave.addTrip()

        var lastIndex = cavePageArea.currentCave.rowCount() - 1;
        var lastModelIndex = cavePageArea.currentCave.index(lastIndex);
        var lastTrip = cavePageArea.currentCave.data(lastModelIndex, Cave.TripObjectRole);

        RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page,
                                                   cavePageArea.tripPageName(lastTrip));
        return lastTrip;
    }

    // Add Trip → Add trip from survey file…: create the trip first
    // (the dialog targets an existing trip), but don't navigate - the
    // dialog's outcome decides. attached()/imported() name the trip
    // after the picked file and navigate; dismissed() deletes the
    // orphan, since the user asked for a trip from a file, not an
    // empty native one.
    function addTripFromSurveyFileWithDialog() {
        cavePageArea.currentCave.addTrip()
        var newTrip = cavePageArea.currentCave.trip(cavePageArea.currentCave.rowCount() - 1)
        addSurveyFileDialogId.trip = newTrip
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
        newTrip.name = cavePageArea.currentCave.uniqueTripName(baseName)
        RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page,
                                                   cavePageArea.tripPageName(newTrip))
    }

    function registerSubPages() {
        if(currentCave) {
            var oldCarpetPage = PageView.page.childPage("Leads")
            if(oldCarpetPage !== RootData.pageSelectionModel.currentPage) {
                if(oldCarpetPage !== null) {
                    RootData.pageSelectionModel.unregisterPage(oldCarpetPage)
                }

                if(PageView.page.name !== "Leads") {
                    RootData.pageSelectionModel.registerPage(PageView.page,
                                                             "Leads",
                                                             caveLeadsPage,
                                                             {"cave":currentCave});
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
                                                             {"cave":currentCave});
                }
            }
        }
    }

    PageView.onPageChanged: registerSubPages()

    onCurrentCaveChanged: {
        instantiatorId.model = cavePageArea.currentCave
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

    // The narrow layout's Add Trip bar lives inside narrowColumnComponent,
    // whose ids aren't reachable from out here; it publishes itself on
    // completion so the one hint can point at whichever bar is showing.
    property QQ.Item narrowAddTripBar: null

    // currentCave.tripCount rather than currentCave.rowCount(): rowCount()
    // is a plain function, so a binding on it would never re-evaluate when a
    // trip arrives, while tripCount announces itself.
    readonly property bool hasNoTrips: cavePageArea.currentCave !== null
                                       && cavePageArea.currentCave.tripCount === 0

    // The tree shows a cave's trips, so it stays away while the page stands
    // for no cave: a null rootNode is a whole-region tree, which is the Data
    // page's reading of it and no cave page's.
    readonly property bool showsTripTree: cavePageArea.currentCave !== null
                                          && !cavePageArea.hasNoTrips

    // True while this cave's centerline comes from an attached survey
    // file. The trips such a cave holds are windows into that file, so
    // the page shows the attachment's own state and stops inviting
    // native trips.
    readonly property bool caveAttached: cavePageArea.currentCave !== null
                                         && cavePageArea.currentCave.externalCenterline.entryFile.length > 0

    // Whether the warnings banner has anything to list. Drives the banner
    // proxies' visibility directly — the banner's own visibility is controlled
    // by whichever proxy hosts it, so this reads the banner's count and never
    // its visible (that clobbers the proxy's imperative control and deadlocks).
    readonly property bool hasWarnings: warningsBannerId.count > 0

    // Brings the attached file's source line into view: the wide page scrolls
    // as a whole, while the narrow column always shows it under the stats.
    function showSourceLine() {
        if (cavePageArea.isNarrow) {
            return
        }
        const top = caveSummaryId.mapToItem(wideFlickableId.contentItem, 0, 0).y - Theme.pageMargin
        const maxContentY = Math.max(0, wideFlickableId.contentHeight - wideFlickableId.height)
        wideFlickableId.contentY = Math.min(Math.max(0, top), maxContentY)
    }

    // --- Standalone items (defined once, proxied into wide/narrow layouts) ---

    DoubleClickTextInput {
        id: caveNameText
        text: cavePageArea.currentCave ? cavePageArea.currentCave.name : ""
        font.bold: true
        font.pixelSize: Theme.fontSizeTitle
        wrapMode: QQ.Text.WordWrap

        onFinishedEditting: (newText) => {
                                cavePageArea.currentCave.name = newText
                            }
    }

    NodeWarningsBanner {
        id: warningsBannerId

        node: cavePageArea.currentCave

        onSourceLineRequested: cavePageArea.showSourceLine()
    }

    SelectableCaveStat {
        id: lengthStat
        label: "Length:"
        unitValue: cavePageArea.currentCave ? cavePageArea.currentCave.length : null
    }

    SelectableCaveStat {
        id: depthStat
        label: "Depth:"
        unitValue: cavePageArea.currentCave ? cavePageArea.currentCave.depth : null
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
                RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page, "Leads");
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
            text: cavePageArea.currentCave ? cavePageArea.currentCave.fixStations.count : 0
            onClicked: {
                RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page, "Fix Stations");
            }
        }

        FixStationErrorBadge {
            objectName: "fixStationsBadge"
            errorModel: cavePageArea.currentCave ? cavePageArea.currentCave.errorModel : null
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
                text: cavePageArea.currentCave ? cavePageArea.currentCave.gridConvergence.text : ""

                QQ.HoverHandler {
                    id: gridConvergenceHoverId
                }

                // The n/a readings have nothing the label doesn't already say —
                // detailText repeats it verbatim — so only a real angle earns a
                // tooltip.
                QC.ToolTip.visible: gridConvergenceHoverId.hovered
                                    && cavePageArea.currentCave
                                    && cavePageArea.currentCave.gridConvergence.state === GridConvergence.Valid
                QC.ToolTip.text: cavePageArea.currentCave ? cavePageArea.currentCave.gridConvergence.detailText : ""
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
        cave: cavePageArea.currentCave
    }

    // The cave's trips, in the same tree the Data page shows, rooted at this
    // cave: its trips are the view's own rows. One instance serves both
    // layouts through the proxies below, so the narrow page shows the tree the
    // wide page shows rather than a list of its own.
    SurveyTreeView {
        id: tripTreeId
        objectName: "tripTree"

        removeAskBox: removeChallengeId
        rootNode: cavePageArea.currentCave
        // The wide page scrolls as a whole, so the tree gives up its own
        // scrolling there and takes the height every row needs. The narrow
        // page hands it what is left of the window and lets it scroll.
        scrollable: cavePageArea.isNarrow

        // Set here rather than on the proxies: a LayoutItemProxy forwards its
        // target's Layout properties, so one set of them serves both layouts.
        Layout.fillWidth: true
        Layout.fillHeight: cavePageArea.isNarrow
        Layout.preferredHeight: cavePageArea.isNarrow ? -1 : tripTreeId.fullHeight
    }

    QQ.Flow {
        id: actionBar
        spacing: Theme.actionBarSpacing
        Layout.fillWidth: true

        AddAndSearchBar {
            id: addTripBarWideId
            objectName: "addTrip"
            addButtonText: "Add Trip"
            menu: addTripMenuId
            menuToolTip: qsTr("More ways to add a trip")
            onAdd: cavePageArea.addTripAndNavigate()
        }

        ExportImportButtons {
            id: exportButton
            objectName: "exportImportButtons"
            visible: RootData.desktopBuild && !cavePageArea.hasNoTrips
            currentRegion: RootData.region
            currentCave: cavePageArea.currentCave
            // The tree's current row, which the export verbs take a trip
            // from. A cave row leaves them without one.
            currentTrip: tripTreeId.currentObject as Trip
        }
    }

    // Sits outside both layouts and positions itself, so one hint serves
    // wide and narrow. The arrow points at Add Trip, which is why the
    // text doesn't name the button.
    HelpQuoteBox {
        id: noTripsHintId
        objectName: "noTripsHint"

        readonly property QQ.Item targetBar: cavePageArea.isNarrow
                                             ? cavePageArea.narrowAddTripBar
                                             : addTripBarWideId

        z: 10
        triangleOffset: 0.0
        // The hint is a sibling of the scrolling area it points into, so
        // it has to retire itself when the bar scrolls out of view.
        visibilityClip: cavePageArea
        // Names the control rather than drawing a glyph: the button shows a
        // stroked chevron, not the solid triangle a "▾" renders, and a bare
        // symbol inside qsTr has no font-coverage or translator guarantee.
        text: qsTr("No trips yet — add one here, or use the menu beside this button to add one from a survey file.")
        visible: cavePageArea.hasNoTrips && !cavePageArea.caveAttached
                 && noTripsHintId.targetBar !== null
        pointAtObject: noTripsHintId.targetBar
        pointAtObjectPosition: noTripsHintId.targetBar !== null
                               ? Qt.point(noTripsHintId.targetBar.width / 2.0,
                                          noTripsHintId.targetBar.height)
                               : Qt.point(0, 0)
    }

    QC.Menu {
        id: addTripMenuId
        objectName: "addTripMenu"

        QC.MenuItem {
            objectName: "addExternalTripMenuItem"
            text: qsTr("Add trip from survey file…")
            onTriggered: cavePageArea.addTripFromSurveyFileWithDialog()
        }
    }

    AddSurveyFileDialog {
        id: addSurveyFileDialogId

        onAttached: {
            let newTrip = addSurveyFileDialogId.trip
            if (newTrip === null) {
                return
            }
            cavePageArea.nameTripFromFileAndNavigate(
                newTrip, newTrip.externalCenterline.entryFile)
        }

        onImported: (sourcePath) => {
            let newTrip = addSurveyFileDialogId.trip
            if (newTrip === null) {
                return
            }
            cavePageArea.nameTripFromFileAndNavigate(newTrip, sourcePath)
        }

        onDismissed: {
            let orphanTrip = addSurveyFileDialogId.trip
            addSurveyFileDialogId.trip = null
            if (orphanTrip === null || cavePageArea.currentCave === null) {
                return
            }
            let index = cavePageArea.currentCave.indexOf(orphanTrip)
            if (index >= 0) {
                cavePageArea.currentCave.removeTrip(index)
            }
        }
    }

    QQ.Loader {
        id: narrowLoaderId
        active: cavePageArea.isNarrow
        visible: cavePageArea.isNarrow
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
        objectName: "cavePageWideFlickable"
        visible: !cavePageArea.isNarrow
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
                Layout.minimumWidth: statsColumnId.implicitWidth + Theme.statsPadding * 2
                Layout.maximumWidth: Theme.infoColumnMaxWidth
                Layout.alignment: Qt.AlignTop
                spacing: Theme.flowSpacing

                LayoutItemProxy { target: caveNameText }

                LayoutItemProxy {
                    objectName: "nodeWarningsBannerProxy"
                    target: cavePageArea.isNarrow ? null : warningsBannerId
                    // Hide the proxy when there is no warning: a visible proxy
                    // whose target is invisible still forwards the target's
                    // implicit height, reserving an empty full-width slot (an
                    // empty "badge"). An invisible layout item is excluded.
                    visible: cavePageArea.hasWarnings
                    Layout.fillWidth: true
                }

                QQ.Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: statsColumnId.implicitHeight + Theme.statsPadding * 2
                    color: Theme.borderSubtle

                    ColumnLayout {
                        id: statsColumnId
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Theme.statsPadding
                        spacing: Theme.tightSpacing

                        LayoutItemProxy { target: lengthStat }
                        LayoutItemProxy { target: depthStat }

                        QQ.Item { implicitHeight: Theme.delegatePadding }

                        LayoutItemProxy { target: leadsRow }
                        LayoutItemProxy { target: fixStationsRow }
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
                    visible: cavePageArea.caveAttached && !cavePageArea.isNarrow
                }

                // An empty cave has nothing to tabulate, so the tree stays
                // out of the hint's way. Both proxies hidden hides the tree
                // itself, which is how a proxied item is put away.
                LayoutItemProxy {
                    target: tripTreeId
                    visible: !cavePageArea.isNarrow && cavePageArea.showsTripTree
                }
            }
        }
    }

    // --- Narrow layout ---
    // The column itself is narrowColumnComponent, below.
    QQ.Item {
        visible: cavePageArea.isNarrow
        anchors.fill: parent
        anchors.margins: Theme.pageMargin

        LayoutItemProxy { target: narrowLoaderId; anchors.fill: parent }
    }

    LeadModel {
        id: leadModelId
        regionModel: RootData.regionTreeModel
        cave: cavePageArea.currentCave
    }

    // The narrow column: the same tree the wide page shows, under the cave's
    // stats — what the wide layout puts in two columns, stacked, with the tree
    // scrolling itself rather than riding a page-wide Flickable.
    QQ.Component {
        id: narrowColumnComponent

        ColumnLayout {
            spacing: Theme.sectionSpacing

            DoubleClickTextInput {
                text: cavePageArea.currentCave ? cavePageArea.currentCave.name : ""
                font.bold: true
                font.pixelSize: Theme.fontSizeTitle

                onFinishedEditting: (newText) => {
                                        cavePageArea.currentCave.name = newText
                                    }
            }

            LayoutItemProxy {
                target: cavePageArea.isNarrow ? warningsBannerId : null
                visible: cavePageArea.hasWarnings
                Layout.fillWidth: true
            }

            QQ.Flow {
                Layout.fillWidth: true
                spacing: Theme.flowSpacing

                SelectableCaveStat {
                    label: "Length:"
                    unitValue: cavePageArea.currentCave ? cavePageArea.currentCave.length : null
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                SelectableCaveStat {
                    label: "Depth:"
                    unitValue: cavePageArea.currentCave ? cavePageArea.currentCave.depth : null
                    depth: true
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                RowLayout {
                    spacing: Theme.delegatePadding

                    QC.Label { text: "Leads:" }

                    LinkText {
                        text: leadModelId.count
                        onClicked: {
                            RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page, "Leads");
                        }
                    }
                }

                QC.Label { text: "·"; color: Theme.textSubtle }

                RowLayout {
                    spacing: Theme.delegatePadding

                    QC.Label { text: "Fix stations:" }

                    LinkText {
                        text: cavePageArea.currentCave ? cavePageArea.currentCave.fixStations.count : 0
                        onClicked: {
                            RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page, "Fix Stations");
                        }
                    }

                    FixStationErrorBadge {
                        errorModel: cavePageArea.currentCave ? cavePageArea.currentCave.errorModel : null
                        errorTypeIds: RootData.region.fixStationValidator.fixStationErrorTypeIds
                    }
                }
            }

            AddAndSearchBar {
                id: addTripBarNarrowId
                objectName: "addTrip"
                addButtonText: "Add Trip"
                menu: addTripMenuId
                menuToolTip: qsTr("More ways to add a trip")
                onAdd: cavePageArea.addTripAndNavigate()

                QQ.Component.onCompleted: cavePageArea.narrowAddTripBar = addTripBarNarrowId
                QQ.Component.onDestruction: cavePageArea.narrowAddTripBar = null
            }

            LayoutItemProxy {
                target: caveSummaryId
                visible: cavePageArea.caveAttached && cavePageArea.isNarrow
            }

            LayoutItemProxy {
                target: tripTreeId
                visible: cavePageArea.isNarrow && cavePageArea.showsTripTree
            }

            //Holds the tree up when an empty cave leaves it out, so the
            //hint keeps the space it points into.
            QQ.Item {
                Layout.fillHeight: cavePageArea.hasNoTrips
            }
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
                           var page = RootData.pageSelectionModel.registerPage(cavePageArea.PageView.page, //From
                                                                               cavePageArea.tripPageName(trip), //Name
                                                                               tripPageComponent, //component
                                                                               {"currentTrip":trip}
                                                                               )
                           object.page = page;
                           page.setNamingFunction(trip, //The trip that's signaling
                                                  "nameChanged()", //Signal
                                                  cavePageArea, //The object that has renaming function
                                                  "tripPageName", //The function that will generate the name
                                                  trip) //The paramaters to tripPageName() function
                       }

        onObjectRemoved: (index, object) => {
                             RootData.pageSelectionModel.unregisterPage((object as Delegate).page);
                         }
    }

    QQ.Component {
        id: tripPageComponent
        TripPage {
            anchors.fill: parent
        }
    }
}
