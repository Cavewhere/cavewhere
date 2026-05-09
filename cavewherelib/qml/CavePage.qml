/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// pragma ComponentBehavior: Bound

import QtQuick as QQ
import QtQuick.Dialogs as QD
import cavewherelib
import QtQml
import QtQuick.Layouts
import QtQuick.Controls as QC
import "Utils.js" as Utils

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

    function importSurvexAsNewTrip(fileUrl) {
        var newTrip = addTripAndNavigate()
        RootData.surveyImportManager.importSurvexToTrip(fileUrl, newTrip)
    }

    function registerSubPages() {
        if(currentCave) {
            var oldCarpetPage = PageView.page.childPage("Leads")
            if(oldCarpetPage !== RootData.pageSelectionModel.currentPage) {
                if(oldCarpetPage !== null) {
                    RootData.pageSelectionModel.unregisterPage(oldCarpetPage)
                }

                if(PageView.page.name !== "Leads") {
                    var page = RootData.pageSelectionModel.registerPage(PageView.page,
                                                                        "Leads",
                                                                        caveLeadsPage,
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

    readonly property bool isNarrow: width < Theme.breakpointPanelCollapse

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

    SelectableCaveStat {
        id: lengthStat
        label: "Length:"
        unitValue: cavePageArea.currentCave ? cavePageArea.currentCave.length : null
        unitModel: UnitDefaults.lengthModel
    }

    SelectableCaveStat {
        id: depthStat
        label: "Depth:"
        unitValue: cavePageArea.currentCave ? cavePageArea.currentCave.depth : null
        unitModel: UnitDefaults.depthModel
    }

    RowLayout {
        id: leadsRow
        spacing: Theme.delegatePadding

        QC.Label {
            text: "Leads:"
        }

        LinkText {
            objectName: "leadsLink"
            text: leadModelId.rowCount()
            onClicked: {
                RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page, "Leads");
            }
        }
    }

    ColumnLayout {
        id: fixStationsBlock

        readonly property int fixCount: cavePageArea.currentCave
                                        ? cavePageArea.currentCave.fixStations.count
                                        : 0

        spacing: Theme.tightSpacing

        QC.Label {
            text: "Fix stations: " + fixStationsBlock.fixCount
        }

        QQ.ListView {
            Layout.fillWidth: true
            implicitHeight: Math.min(contentHeight, 4 * Theme.fontSizeBody * 1.5)
            visible: fixStationsBlock.fixCount > 0
            model: cavePageArea.currentCave ? cavePageArea.currentCave.fixStations : null
            interactive: false
            delegate: QC.Label {
                required property string stationName
                required property string inputCS
                required property double easting
                required property double northing
                required property double elevation
                text: "%1  %2  (%3, %4, %5)"
                    .arg(stationName)
                    .arg(inputCS)
                    .arg(easting)
                    .arg(northing)
                    .arg(elevation)
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }

    QQ.Flow {
        id: actionBar
        spacing: Theme.actionBarSpacing
        Layout.fillWidth: true

        AddAndSearchBar {
            objectName: "addTrip"
            addButtonText: "Add Trip"
            onAdd: cavePageArea.addTripAndNavigate()
        }

        QC.Button {
            objectName: "importSurvexButton"
            text: "Import Survex"
            onClicked: survexImportDialogId.open()
        }

        ExportImportButtons {
            id: exportButton
            visible: RootData.desktopBuild
            currentRegion: RootData.region
            currentCave: cavePageArea.currentCave
            currentTrip: {
                if(cavePageArea.isNarrow) return null;
                let wideItem = wideLoaderId.item
                if(!wideItem) return null;
                let tv = wideItem.tripTableView
                if(!tv) return null;
                let row = tv.currentItem as RowDelegate
                return row ? row.tripObjectRole : null
            }
        }
    }

    QD.FileDialog {
        id: survexImportDialogId
        title: "Import Survex (.svx)"
        nameFilters: [ "Survex files (*.svx)", "All files (*)" ]
        currentFolder: RootData.lastDirectory
        fileMode: QD.FileDialog.OpenFile
        onAccepted: {
            RootData.lastDirectory = selectedFile
            cavePageArea.importSurvexAsNewTrip(selectedFile)
        }
    }

    QQ.Loader {
        id: wideLoaderId
        active: !cavePageArea.isNarrow
        visible: !cavePageArea.isNarrow
        Layout.fillWidth: true
        Layout.fillHeight: true
        sourceComponent: wideTableComponent
    }

    QQ.Loader {
        id: narrowLoaderId
        active: cavePageArea.isNarrow
        visible: cavePageArea.isNarrow
        Layout.fillWidth: true
        Layout.fillHeight: true
        sourceComponent: narrowListComponent
    }

    // --- Wide layout ---
    RowLayout {
        id: wideLayoutId
        visible: !cavePageArea.isNarrow
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        spacing: Theme.columnGap

        ColumnLayout {
            Layout.minimumWidth: statsColumnId.implicitWidth + Theme.statsPadding
            Layout.maximumWidth: Theme.infoColumnMaxWidth
            Layout.alignment: Qt.AlignTop
            spacing: Theme.flowSpacing

            LayoutItemProxy { target: caveNameText }

            QQ.Rectangle {
                Layout.fillWidth: true
                implicitHeight: statsColumnId.implicitHeight + Theme.statsPadding
                color: Theme.borderSubtle

                ColumnLayout {
                    id: statsColumnId
                    anchors.centerIn: parent
                    spacing: Theme.tightSpacing

                    LayoutItemProxy { target: lengthStat }
                    LayoutItemProxy { target: depthStat }

                    QQ.Item { implicitHeight: Theme.delegatePadding }

                    LayoutItemProxy { target: leadsRow }
                    LayoutItemProxy { target: fixStationsBlock }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.sectionSpacing

            LayoutItemProxy { target: actionBar }
            LayoutItemProxy { target: wideLoaderId }
        }
    }

    // --- Narrow layout ---
    QQ.Item {
        visible: cavePageArea.isNarrow
        anchors.fill: parent
        anchors.margins: Theme.pageMargin

        LayoutItemProxy { target: narrowLoaderId; anchors.fill: parent }
    }

    SortFilterProxyModel {
        id: tripProxyModel
        source: CavePageModel {
            cave: cavePageArea.currentCave
        }
    }

    LeadModel {
        id: leadModelId
        regionModel: RootData.regionTreeModel
        cave: cavePageArea.currentCave
    }

    QQ.Component {
        id: wideTableComponent

        ColumnLayout {
            spacing: 0

            property alias tripTableView: tableViewId

                TableStaticColumnModel {
                    id: columnModelId
                    columns: [
                        TableStaticColumn {
                            id: nameColumn
                            columnWidth: 200
                            text: "Name"
                            sortRole: CavePageModel.TripNameRole
                        },
                        TableStaticColumn {
                            id: dateColumn
                            columnWidth: 75
                            text: "Date"
                            sortRole: CavePageModel.TripDateRole
                        },
                        TableStaticColumn {
                            id: stationsColumn
                            columnWidth: 75
                            text: "Stations"
                            sortRole: CavePageModel.UsedStationsRole
                        },
                        TableStaticColumn {
                            id: lengthColumn
                            columnWidth: 50
                            text: "Length"
                            sortRole: CavePageModel.TripDistanceRole
                        }
                    ]
                }

                HorizontalHeaderStaticView {
                    view: tableViewId
                    Layout.fillWidth: true

                    delegate: TableStaticHeaderColumn {
                        model: tableViewId.model
                    }
                }

                QC.ScrollView {
                    id: scrollViewId
                    implicitWidth: tableViewId.implicitWidth +
                                   QC.ScrollBar.vertical.implicitWidth
                    Layout.fillHeight: true

                    TableStaticView {
                        id: tableViewId
                        objectName: "tripTableView"
                        model: tripProxyModel
                        columnModel: columnModelId
                        Layout.fillHeight: true

                        component RowDelegate : QQ.Item {
                            id: rowDelegateId
                            required property Trip tripObjectRole
                            required property string tripNameRole
                            required property date tripDateRole
                            required property string usedStationsRole
                            required property real tripDistanceRole
                            required property int index

                            implicitWidth: layoutId.width
                            implicitHeight: layoutId.height

                            DataRightClickMouseMenu {
                                anchors.fill: parent
                                removeChallenge: removeChallengeId
                                row: rowDelegateId.index
                                name: rowDelegateId.tripNameRole
                            }

                            TableRowBackground {
                                isSelected: tableViewId.currentIndex === rowDelegateId.index
                                rowIndex: rowDelegateId.index
                                anchors.fill: parent
                            }

                            QQ.MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton

                                onClicked: {
                                    tableViewId.currentIndex = rowDelegateId.index
                                }
                            }

                            RowLayout {
                                id: layoutId

                                spacing: 0

                                QQ.Item {
                                    implicitWidth: nameColumn.columnWidth
                                    implicitHeight: rowLayout.height
                                    clip: true

                                    RowLayout {
                                        id: rowLayout
                                        spacing: 1

                                        ErrorIconBar {
                                            errorModel: rowDelegateId.tripObjectRole.errorModel
                                        }

                                        LinkText {
                                            text: rowDelegateId.tripNameRole
                                            elide: QQ.Text.ElideRight

                                            onClicked: {
                                                RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page,
                                                                                           tripPageName(rowDelegateId.tripObjectRole));
                                            }
                                        }
                                    }
                                }

                                QQ.Item {
                                    implicitWidth: dateColumn.columnWidth
                                    implicitHeight: dateId.implicitHeight
                                    clip: true
                                    QC.Label {
                                        id: dateId
                                        elide: QQ.Text.ElideRight
                                        text: Qt.formatDateTime(rowDelegateId.tripDateRole, "yyyy-MM-dd")
                                    }
                                }

                                QQ.Item {
                                    implicitWidth: stationsColumn.columnWidth
                                    implicitHeight: usedStationsId.implicitHeight
                                    clip: true

                                    QC.Label {
                                        id: usedStationsId
                                        elide: QQ.Text.ElideRight
                                        text: usedStationsRole
                                    }
                                }

                                QQ.Item {
                                    implicitWidth: lengthColumn.columnWidth
                                    implicitHeight: lengthId.implicitHeight
                                    clip: true

                                    QC.Label {
                                        id: lengthId
                                        elide: QQ.Text.ElideRight
                                        text: {
                                            var unit = ""
                                            switch(rowDelegateId.tripObjectRole.calibration.distanceUnit) {
                                            case Units.Meters:
                                                unit = "m"
                                                break;
                                            case Units.Feet:
                                                unit = "ft"
                                                break;
                                            }

                                            return Utils.fixed(rowDelegateId.tripDistanceRole, 2) + " " + unit;
                                        }
                                    }
                                }
                            }
                        }

                        delegate: RowDelegate {}
                    }
                }
            }

    }

    QQ.Component {
        id: narrowListComponent

        QQ.ListView {
            clip: true
            model: tripProxyModel

            header: ColumnLayout {
                width: parent ? parent.width : 0
                spacing: Theme.sectionSpacing

                DoubleClickTextInput {
                    text: cavePageArea.currentCave ? cavePageArea.currentCave.name : ""
                    font.bold: true
                    font.pixelSize: Theme.fontSizeTitle

                    onFinishedEditting: (newText) => {
                                            cavePageArea.currentCave.name = newText
                                        }
                }

                QQ.Flow {
                    Layout.fillWidth: true
                    spacing: Theme.flowSpacing

                    SelectableCaveStat {
                        label: "Length:"
                        unitValue: cavePageArea.currentCave ? cavePageArea.currentCave.length : null
                        unitModel: UnitDefaults.lengthModel
                    }

                    QC.Label { text: "·"; color: Theme.textSubtle }

                    SelectableCaveStat {
                        label: "Depth:"
                        unitValue: cavePageArea.currentCave ? cavePageArea.currentCave.depth : null
                        unitModel: UnitDefaults.depthModel
                    }

                    QC.Label { text: "·"; color: Theme.textSubtle }

                    RowLayout {
                        spacing: Theme.delegatePadding

                        QC.Label { text: "Leads:" }

                        LinkText {
                            text: leadModelId.rowCount()
                            onClicked: {
                                RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page, "Leads");
                            }
                        }
                    }
                }

                QQ.Flow {
                    Layout.fillWidth: true
                    spacing: Theme.actionBarSpacing

                    AddAndSearchBar {
                        objectName: "addTrip"
                        addButtonText: "Add Trip"
                        onAdd: cavePageArea.addTripAndNavigate()
                    }

                    QC.Button {
                        objectName: "importSurvexButton"
                        text: "Import Survex"
                        onClicked: survexImportDialogId.open()
                    }

                    RowLayout {
                        spacing: Theme.delegatePadding

                        QC.ComboBox {
                            id: narrowSortComboId
                            model: ["Name", "Date", "Stations", "Length"]

                            property list<int> sortRoles: [
                                CavePageModel.TripNameRole,
                                CavePageModel.TripDateRole,
                                CavePageModel.UsedStationsRole,
                                CavePageModel.TripDistanceRole
                            ]

                            onActivated: {
                                tripProxyModel.sortRole = sortRoles[currentIndex]
                                tripProxyModel.sort(narrowSortOrderButtonId.ascending ? Qt.AscendingOrder : Qt.DescendingOrder)
                            }
                        }

                        QC.RoundButton {
                            id: narrowSortOrderButtonId
                            property bool ascending: true

                            implicitHeight: narrowSortComboId.height
                            implicitWidth: implicitHeight

                            icon.source: narrowSortOrderButtonId.ascending
                                         ? "qrc:/twbs-icons/icons/sort-up.svg"
                                         : "qrc:/twbs-icons/icons/sort-down.svg"
                            icon.width: Theme.iconSizeButton
                            icon.height: Theme.iconSizeButton
                            icon.color: Theme.text

                            onClicked: {
                                ascending = !ascending
                                tripProxyModel.sortRole = narrowSortComboId.sortRoles[narrowSortComboId.currentIndex]
                                tripProxyModel.sort(ascending ? Qt.AscendingOrder : Qt.DescendingOrder)
                            }
                        }
                    }
                }
            }

            delegate: QQ.Item {
                id: flowDelegateId
                required property Trip tripObjectRole
                required property string tripNameRole
                required property date tripDateRole
                required property string usedStationsRole
                required property real tripDistanceRole
                required property int index

                implicitHeight: flowId.implicitHeight + Theme.delegatePadding
                width: QQ.ListView.view ? QQ.ListView.view.width : 0

                TableRowBackground {
                    isSelected: QQ.ListView.view && QQ.ListView.view.currentIndex === flowDelegateId.index
                    rowIndex: flowDelegateId.index
                    anchors.fill: parent
                }

                QQ.MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: QQ.ListView.view.currentIndex = flowDelegateId.index
                }

                QQ.Flow {
                    id: flowId
                    width: parent.width
                    spacing: Theme.delegatePadding
                    anchors.verticalCenter: parent.verticalCenter

                    ErrorIconBar {
                        errorModel: flowDelegateId.tripObjectRole.errorModel
                    }

                    LinkText {
                        text: flowDelegateId.tripNameRole
                        onClicked: {
                            RootData.pageSelectionModel.gotoPageByName(
                                        cavePageArea.PageView.page,
                                        cavePageArea.tripPageName(flowDelegateId.tripObjectRole))
                        }
                    }

                    QC.Label { text: "·"; color: Theme.textSubtle }
                    QC.Label { text: Qt.formatDateTime(flowDelegateId.tripDateRole, "yyyy-MM-dd") }
                    QC.Label { text: "·"; color: Theme.textSubtle }
                    QC.Label { text: flowDelegateId.usedStationsRole; color: Theme.textSubtle }
                    QC.Label {
                        text: {
                            var unit = ""
                            switch(flowDelegateId.tripObjectRole.calibration.distanceUnit) {
                            case Units.Meters:
                                unit = "m"
                                break;
                            case Units.Feet:
                                unit = "ft"
                                break;
                            }
                            return Utils.fixed(flowDelegateId.tripDistanceRole, 2) + " " + unit
                        }
                        color: Theme.textSubtle
                    }
                }

                DataRightClickMouseMenu {
                    anchors.fill: parent
                    removeChallenge: removeChallengeId
                    row: flowDelegateId.index
                    name: flowDelegateId.tripNameRole
                }
            }
        }
    }


    RemoveAskBox {
        id: removeChallengeId
        onRemove: {
            let proxyIndex = tableViewId.model.index(indexToRemove, 0)
            let sourceIndex = tableViewId.model.mapToSource(proxyIndex)
            cavePageArea.currentCave.removeTrip(sourceIndex.row)
        }
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
