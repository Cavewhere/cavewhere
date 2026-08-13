/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//This cause a crash when creating the context for sub trip pages
//cwPageView: "qrc:/qt/qml/cavewherelib/qml/DataMainPage.qml: Cannot instantiate bound component outside its creation context"
// pragma ComponentBehavior: Bound

import cavewherelib
import QtQml
import QtQuick as QQ
import QtQuick.Layouts
import QtQuick.Controls as QC
import "Utils.js" as Utils

StandardPage {
    id: pageId

    objectName: "dataMainPage"

    readonly property bool isNarrow: width < Theme.breakpointPanelCollapse

    function cavePageName(cave) {
        return "Cave=" + cave.name;
    }

    function registerGeospatialPage() {
        if (PageView.page === null) return
        if (PageView.page.childPage("Geospatial Layers") !== null) return
        RootData.pageSelectionModel.registerPage(PageView.page,
                                                 "Geospatial Layers",
                                                 geospatialLayerPageComponent);
    }

    PageView.onPageChanged: registerGeospatialPage()

    // cwPageSelectionModel::clearHistory() (invoked by newProject/loadProject)
    // wipes "Data"'s child pages but keeps DataMainPage cached, so
    // PageView.onPageChanged won't re-fire. Listen on the global signal so
    // the link is reachable again after a project reset.
    Connections {
        target: RootData.pageSelectionModel
        function onCurrentPageChanged() {
            pageId.registerGeospatialPage()
            // Leaving (or re-entering) the page drops the settings back to
            // read-only so the rarely-changed unit/CS editors are never left
            // exposed after navigating away.
            settingsEditButton.editMode = false
        }
    }

    QQ.Component {
        id: geospatialLayerPageComponent
        GeospatialLayerPage {
            anchors.fill: parent
        }
    }

    // --- Standalone items (defined once, proxied into wide/narrow layouts) ---

    RowLayout {
        id: titleRow
        Layout.fillWidth: true
        spacing: Theme.flowSpacing

        DoubleClickTextInput {
            objectName: "regionNameInput"
            font.bold: true
            font.pixelSize: Theme.fontSizeTitle
            text: RootData.region.name

            onFinishedEditting: (newText) => {
                RootData.region.name = newText
            }
        }

        QQ.Item { Layout.fillWidth: true }

        ContextMenuButton {
            objectName: "regionContextMenu"
            iconSource: "qrc:/twbs-icons/icons/list.svg"
            // ContextMenuButton defaults to a 20×20 caret for field-level
            // dropdowns; size up here so the page-level hamburger reads
            // as a primary action.
            implicitWidth: Theme.iconSizeMedium
            implicitHeight: Theme.iconSizeMedium
            icon.width: Theme.iconSizeSmall
            icon.height: Theme.iconSizeSmall
            menu: regionContextMenuComponent
        }
    }

    // The unit system is a project-wide choice a user rarely changes but can
    // wreck a project by flipping. It shows read-only until the user clicks
    // Edit, which is the extra click the design asks for.
    QQ.Rectangle {
        id: regionInfoBox
        objectName: "regionInfoBox"

        property bool editMode: settingsEditButton.editMode

        // 6 decimals of a degree is ~0.1 m, far finer than an origin that only
        // has to say which valley the project sits in. The picker reads to 8
        // because it reports a point the user placed; this reports a projection
        // parameter.
        readonly property int originPrecision: 6

        Layout.fillWidth: true
        implicitHeight: infoColumnId.implicitHeight + Theme.statsPadding * 2
        color: Theme.borderSubtle

        function formatOrigin(latitude, longitude) {
            return Utils.formatLatLon(latitude, longitude, regionInfoBox.originPrecision)
        }

        ColumnLayout {
            id: infoColumnId
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.statsPadding
            spacing: Theme.tightSpacing

            QC.Label {
                text: qsTr("Project")
                font.bold: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.tightSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.delegatePadding

                    LabelWithHelp {
                        objectName: "gisSourcesLabel"
                        text: qsTr("GIS Sources:")
                        helpArea: gisSourcesHelpArea
                    }

                    LinkText {
                        objectName: "geospatialLayersLink"
                        text: RootData.region.lazLayers.count
                        onClicked: {
                            RootData.pageSelectionModel.gotoPageByName(pageId.PageView.page,
                                                                       "Geospatial Layers");
                        }
                    }

                    QQ.Item { Layout.fillWidth: true }
                }

                HelpArea {
                    id: gisSourcesHelpArea
                    objectName: "gisSourcesHelp"
                    Layout.fillWidth: true
                    text: "<p>A <b>GIS source</b> is a georeferenced file CaveWhere can draw " +
                          "with your survey. Currently, only LiDAR point clouds are " +
                          "supported, <b>.laz</b> or <b>.las</b>.</p>" +
                          "<p>Click the number to add or remove files.</p>"
                }
            }

            // Everything the project's coordinates are expressed in, read as one
            // group: the units they are shown in, where the frame sits, what it
            // is centered on, and what it is measured against. Units is the only
            // one of them you pick. The projection rows are read-only on
            // purpose: CaveWhere derives its own projection from the first thing
            // you georeference and inherits that input's datum, so there is
            // nothing there to choose — a datum chosen by hand could only
            // disagree with the data it describes.
            //
            // Edit rides the group's title line because Units is the only thing
            // in the box it unlocks.
            QC.GroupBox {
                id: coordinateSystemGroup
                objectName: "coordinateSystemGroup"

                Layout.fillWidth: true
                Layout.topMargin: Theme.flowSpacing

                // A custom label replaces the one the style positions and
                // measures, so it has to do both jobs itself. It sits at the
                // frame's left padding to line up with the rows, and it declares
                // an implicit size: the style reserves the title's room only when
                // the label reports an implicit width, and reserves the height it
                // reports — a label that stays implicitly empty gets no room and
                // prints over the first row.
                label: QQ.Item {
                    x: coordinateSystemGroup.leftPadding
                    width: coordinateSystemGroup.availableWidth
                    implicitWidth: coordinateSystemTitle.implicitWidth
                                   + Theme.flowSpacing
                                   + settingsEditButton.implicitWidth
                    implicitHeight: Math.max(coordinateSystemTitle.implicitHeight,
                                             settingsEditButton.implicitHeight)

                    QC.Label {
                        id: coordinateSystemTitle
                        objectName: "coordinateSystemTitle"

                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter

                        text: qsTr("Coordinate System")
                        font.bold: true
                    }

                    EditToggleButton {
                        id: settingsEditButton
                        objectName: "regionSettingsEditButton"

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                ColumnLayout {
                    id: coordinateSystemColumn

                    spacing: Theme.tightSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.delegatePadding

                        QC.Label {
                            text: qsTr("Units:")
                        }

                        QC.Label {
                            objectName: "unitSystemValue"
                            visible: !regionInfoBox.editMode
                            text: Units.unitSystemName(RootData.region.unitSystem)
                        }

                        UnitSystemComboBox {
                            objectName: "unitSystemComboBox"
                            visible: regionInfoBox.editMode
                            // The project-wide unit system (region-level). Seeds new trips
                            // and drives every displayed length; existing trips keep their
                            // entry units. Metric = index 0, Imperial = index 1.
                            currentIndex: RootData.region.unitSystem
                            onActivated: RootData.region.unitSystem = currentIndex
                        }

                        QQ.Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.delegatePadding

                        LabelWithHelp {
                            objectName: "projectionLabel"
                            text: qsTr("Location:")
                            helpArea: projectionHelpArea
                        }

                        QC.Label {
                            objectName: "projectionOriginValue"
                            text: RootData.region.geoReference.hasOrigin
                                  ? regionInfoBox.formatOrigin(RootData.region.geoReference.originLatitude,
                                                               RootData.region.geoReference.originLongitude)
                                  : qsTr("Not georeferenced")
                        }

                        // Recentering re-derives a frame that already exists, so
                        // it has nothing to offer a project nothing has placed
                        // yet.
                        QC.Button {
                            objectName: "recenterButton"
                            visible: regionInfoBox.editMode
                                     && RootData.region.geoReference.hasCoordinateSystem
                            text: qsTr("Recenter…")
                            onClicked: projectionCenterDialogId.open()
                        }

                        QQ.Item { Layout.fillWidth: true }
                    }

                    // Only an Anchored frame has an input answerable for where it
                    // sits. A frozen frame is centered on the middle of the data
                    // and has no one station to name, so the row stands down.
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.delegatePadding
                        visible: RootData.region.geoReference.state === GeoReference.Anchored
                                 && RootData.region.geoReference.anchorDescription !== ""

                        QC.Label {
                            text: qsTr("Centered on:")
                        }

                        QC.Label {
                            objectName: "projectionAnchorValue"
                            text: RootData.region.geoReference.anchorDescription
                        }

                        QQ.Item { Layout.fillWidth: true }
                    }

                    // Once there is a frame, these rows stay put and say what they
                    // know — a row that vanished would leave the reader guessing
                    // whether the project has no vertical datum or the app forgot
                    // to show it.
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.delegatePadding
                        visible: RootData.region.geoReference.hasCoordinateSystem

                        QC.Label {
                            text: qsTr("Datum:")
                        }

                        QC.Label {
                            id: datumValueId
                            objectName: "projectionDatumValue"

                            readonly property bool unnamed: RootData.region.geoReference.datumName === ""

                            text: datumValueId.unnamed
                                  ? qsTr("Unknown")
                                  : RootData.region.geoReference.datumName
                            color: datumValueId.unnamed ? Theme.textSubtle : Theme.text
                            font.italic: datumValueId.unnamed
                        }

                        QQ.Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.delegatePadding
                        visible: RootData.region.geoReference.hasCoordinateSystem

                        QC.Label {
                            text: qsTr("Elevations:")
                        }

                        QC.Label {
                            id: verticalDatumValueId
                            objectName: "projectionVerticalDatumValue"

                            readonly property bool undeclared: RootData.region.geoReference.verticalDatum === ""

                            text: verticalDatumValueId.undeclared
                                  ? qsTr("Not declared by your data")
                                  : RootData.region.geoReference.verticalDatum
                            color: verticalDatumValueId.undeclared ? Theme.textSubtle : Theme.text
                            font.italic: verticalDatumValueId.undeclared
                        }

                        QQ.Item { Layout.fillWidth: true }
                    }

                    HelpArea {
                        id: projectionHelpArea
                        objectName: "projectionHelp"
                        Layout.fillWidth: true
                        text: "<p>CaveWhere builds its own map projection for each project — a " +
                              "<i>low-distortion projection</i> centered on the first thing you " +
                              "georeference, whether that is a fix station or a GIS source. " +
                              "<b>Location</b> is where that center landed, and <b>Centered on</b> " +
                              "names the fix station or GIS source it landed on.</p>" +
                              "<p>Click <b>Edit</b> and then <b>Recenter…</b> to move that center " +
                              "onto a fix station of your choosing, or onto the middle of your " +
                              "data. True north lines up best near the center, so the station in " +
                              "the middle of the part of the cave you care about is a good one to " +
                              "pick. The datum still comes from your data either way.</p>" +
                              "<p>The <b>datum</b> is the model of the Earth's shape your " +
                              "coordinates are measured against, and it comes from that same " +
                              "first input — it is shown here rather than chosen, because every " +
                              "coordinate you enter already says which datum it is on. Plain " +
                              "GPS coordinates are the exception: those say only \"WGS84\", " +
                              "which drifts about 2 cm a year against the continent under you, " +
                              "so CaveWhere uses the national datum for where your cave is " +
                              "instead — <b>NAD83 (National Spatial Reference System 2011)</b> " +
                              "in the US.</p>" +
                              "<p><b>Elevations</b> names the surface heights are measured from, " +
                              "as your LiDAR declared it. CaveWhere passes elevations through " +
                              "exactly as given and never converts between height systems.</p>"
                    }
                }
            }
        }
    }

    QQ.Flow {
        id: actionBar
        Layout.fillWidth: true
        spacing: Theme.actionBarSpacing

        AddAndSearchBar {
            addButtonText: "Add Cave"
            onAdd: {
                RootData.region.addCave();

                var lastIndex = RootData.region.rowCount() - 1;
                var lastModelIndex = RootData.region.index(lastIndex);
                var lastCave = RootData.region.data(lastModelIndex, CavingRegion.CaveObjectRole);

                RootData.pageSelectionModel.gotoPageByName(pageId.PageView.page,
                                                           pageId.cavePageName(lastCave));
            }
        }

        ExportImportButtons {
            id: exportButton
            visible: RootData.desktopBuild
            currentRegion: RootData.region
            importVisible: true
            page: pageId.PageView.page
            currentCave: {
                let currentItem = caveListId.currentItem as CaveDelegate
                if(currentItem) {
                    return currentItem.caveObjectRole
                } else {
                    return null;
                }
            }
        }
    }

    QQ.ListView {
        id: caveListId
        model: RootData.region

        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true

        component CaveDelegate: QQ.Item {
            id: delegateId
            objectName: "caveDelegate" + index

            required property Cave caveObjectRole;
            required property int index

            implicitHeight: flowId.implicitHeight + Theme.delegatePadding
            width: QQ.ListView.view ? QQ.ListView.view.width : 0

            TableRowBackground {
                isSelected: caveListId.currentIndex == delegateId.index
                rowIndex: delegateId.index
                anchors.fill: parent
            }

            QQ.MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton

                onClicked: {
                    caveListId.currentIndex = delegateId.index
                }
            }

            QQ.Flow {
                id: flowId
                width: parent.width
                spacing: 4
                anchors.verticalCenter: parent.verticalCenter

                ErrorIconBar {
                    errorModel: delegateId.caveObjectRole.errorModel
                }

                LinkText {
                    objectName: "caveLink"
                    text: delegateId.caveObjectRole.name
                    onClicked: {
                        RootData.pageSelectionModel.gotoPageByName(pageId.PageView.page,
                                                                   pageId.cavePageName(delegateId.caveObjectRole));
                    }
                }

                QC.Label {
                    text: "is"
                }

                SelectableCaveStat {
                    objectName: "length"
                    unitValue: delegateId.caveObjectRole.length
                }

                QC.Label {
                    text: "long and"
                }

                SelectableCaveStat {
                    objectName: "depth"
                    unitValue: delegateId.caveObjectRole.depth
                    depth: true
                }

                QC.Label {
                    text: "deep"
                }
            }

            DataRightClickMouseMenu {
                anchors.fill: parent
                removeChallenge: removeChallengeId
                name: delegateId.caveObjectRole.name
                row: delegateId.index
            }
        }

        delegate: CaveDelegate {}
    }

    QQ.Component {
        id: regionContextMenuComponent
        QC.Menu {
            QC.MenuItem {
                objectName: "cavernOutputMenuItem"
                text: RootData.linePlotManager.hasSolveError
                      ? qsTr("Cavern Output (solve error)")
                      : qsTr("Cavern Output")
                onTriggered: {
                    // Cavern page is registered at top level — parent
                    // must be null for the lookup to find it.
                    RootData.pageSelectionModel.gotoPageByName(null, "Cavern");
                }
            }
        }
    }

    // --- Wide layout: info column on the left, caves as the main content ---
    QQ.Loader {
        id: wideLayoutLoader
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        active: !pageId.isNarrow
        visible: !pageId.isNarrow
        sourceComponent: wideLayoutComponent
    }

    QQ.Component {
        id: wideLayoutComponent

        RowLayout {
            spacing: Theme.columnGap

            ColumnLayout {
                Layout.maximumWidth: Theme.infoColumnMaxWidth
                Layout.alignment: Qt.AlignTop
                spacing: Theme.sectionSpacing

                LayoutItemProxy { target: titleRow }
                LayoutItemProxy { target: regionInfoBox }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.sectionSpacing

                LayoutItemProxy { target: actionBar }
                LayoutItemProxy { target: caveListId }
            }
        }
    }

    // --- Narrow layout: everything stacked, caves last but still primary ---
    QQ.Loader {
        id: narrowLayoutLoader
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        active: pageId.isNarrow
        visible: pageId.isNarrow
        sourceComponent: narrowLayoutComponent
    }

    QQ.Component {
        id: narrowLayoutComponent

        ColumnLayout {
            spacing: Theme.sectionSpacing

            LayoutItemProxy { target: titleRow }
            LayoutItemProxy { target: regionInfoBox }
            LayoutItemProxy { target: actionBar }
            LayoutItemProxy { target: caveListId }
        }
    }

    ProjectionCenterDialog {
        id: projectionCenterDialogId
    }

    RemoveAskBox {
        id: removeChallengeId
        onRemove: {
            RootData.region.removeCave(indexToRemove);
        }
    }

    Instantiator {
        model: RootData.region
        component PageDelegate : QQ.QtObject {
            id: delegateObjectId
            required property Cave caveObjectRole
            property Page page
        }

        delegate: PageDelegate {}


        onObjectAdded: function (index, object) {
            //In-ables the link

            let delegate = object as PageDelegate
            // console.log(`Cave page added! ${index} ${object.caveObjectRole} ${delegate.caveObjectRole}`)
            var linkId = RootData.pageSelectionModel.registerPage(pageId.PageView.page, //From
                                                                  pageId.cavePageName(delegate.caveObjectRole), //Name
                                                                  caveOverviewPageComponent,
                                                                  {currentCave:delegate.caveObjectRole}
                                                                  )

            delegate.page = linkId
            delegate.page.setNamingFunction(delegate.caveObjectRole,
                                            "nameChanged()",
                                            pageId,
                                            "cavePageName",
                                            delegate.caveObjectRole)
        }

        onObjectRemoved: function (index, object) {
            let delegate = object as PageDelegate
            RootData.pageSelectionModel.unregisterPage(delegate.page);
        }
    }

    //Child page
    QQ.Component {
        id: caveOverviewPageComponent
        CavePage {
            anchors.fill: parent
        }
    }
}
