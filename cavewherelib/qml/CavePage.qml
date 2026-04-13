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
import "Utils.js" as Utils

StandardPage {
    id: cavePageArea
    objectName: "cavePage"

    property Cave currentCave

    function tripPageName(trip) {
        return "Trip=" + trip.name;
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
        text: cavePageArea.currentCave.name
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
        spacing: 4

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

    QQ.Flow {
        id: actionBar
        spacing: 16
        Layout.fillWidth: true

        AddAndSearchBar {
            objectName: "addTrip"
            addButtonText: "Add Trip"
            onAdd: {
                cavePageArea.currentCave.addTrip()

                var lastIndex = cavePageArea.currentCave.rowCount() - 1;
                var lastModelIndex = cavePageArea.currentCave.index(lastIndex);
                var lastTrip = cavePageArea.currentCave.data(lastModelIndex, Cave.TripObjectRole);

                RootData.pageSelectionModel.gotoPageByName(cavePageArea.PageView.page,
                                                           cavePageArea.tripPageName(lastTrip));
            }
        }

        RowLayout {
            visible: cavePageArea.isNarrow
            spacing: 4

            QC.ComboBox {
                id: sortComboId
                model: ["Name", "Date", "Stations", "Length"]

                property list<int> sortRoles: [
                    CavePageModel.TripNameRole,
                    CavePageModel.TripDateRole,
                    CavePageModel.UsedStationsRole,
                    CavePageModel.TripDistanceRole
                ]

                onActivated: {
                    tripProxyModel.sortRole = sortRoles[currentIndex]
                    tripProxyModel.sort(sortOrderButtonId.ascending ? Qt.AscendingOrder : Qt.DescendingOrder)
                }
            }

            QC.RoundButton {
                id: sortOrderButtonId
                property bool ascending: true

                implicitHeight: sortComboId.height
                implicitWidth: implicitHeight

                icon.source: sortOrderButtonId.ascending
                             ? "qrc:/twbs-icons/icons/sort-up.svg"
                             : "qrc:/twbs-icons/icons/sort-down.svg"
                icon.width: 16
                icon.height: 16
                icon.color: Theme.text

                onClicked: {
                    ascending = !ascending
                    tripProxyModel.sortRole = sortComboId.sortRoles[sortComboId.currentIndex]
                    tripProxyModel.sort(ascending ? Qt.AscendingOrder : Qt.DescendingOrder)
                }
            }
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
        spacing: 12

        ColumnLayout {
            Layout.minimumWidth: statsColumnId.implicitWidth + 10
            Layout.maximumWidth: 200
            Layout.alignment: Qt.AlignTop
            spacing: 6

            LayoutItemProxy { target: caveNameText }

            QQ.Rectangle {
                Layout.fillWidth: true
                implicitHeight: statsColumnId.implicitHeight + 10
                color: Theme.borderSubtle

                ColumnLayout {
                    id: statsColumnId
                    anchors.centerIn: parent
                    spacing: 2

                    LayoutItemProxy { target: lengthStat }
                    LayoutItemProxy { target: depthStat }

                    QQ.Item { implicitHeight: 4 }

                    LayoutItemProxy { target: leadsRow }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            LayoutItemProxy { target: actionBar }
            LayoutItemProxy { target: wideLoaderId }
        }
    }

    // --- Narrow layout ---
    ColumnLayout {
        visible: cavePageArea.isNarrow
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        spacing: 8

        LayoutItemProxy { target: caveNameText }

        QQ.Flow {
            Layout.fillWidth: true
            spacing: 6

            LayoutItemProxy { target: lengthStat }
            QC.Label { text: "·"; color: Theme.textSubtle }
            LayoutItemProxy { target: depthStat }
            QC.Label { text: "·"; color: Theme.textSubtle }
            LayoutItemProxy { target: leadsRow }
        }

        LayoutItemProxy { target: actionBar }
        LayoutItemProxy { target: narrowLoaderId }
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
                    spacing: 4
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
