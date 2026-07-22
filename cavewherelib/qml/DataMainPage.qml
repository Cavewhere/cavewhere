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

    // Units and the coordinate system are project-wide choices a user rarely
    // changes but can wreck a project by flipping. They show read-only until
    // the user clicks Edit, which is the extra click the design asks for.
    QQ.Rectangle {
        id: regionInfoBox
        objectName: "regionInfoBox"
        Layout.fillWidth: true
        implicitHeight: infoColumnId.implicitHeight + Theme.statsPadding * 2
        color: Theme.borderSubtle

        property bool editMode: settingsEditButton.editMode

        // The project CS. Its human-facing name/code come from CSFormat (the one
        // presentation source shared with the inline picker). Empty value == Local.
        readonly property string csValue: RootData.region.geoReference.globalCoordinateSystem

        ColumnLayout {
            id: infoColumnId
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.statsPadding
            spacing: Theme.tightSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.flowSpacing

                QC.Label {
                    text: qsTr("Project")
                    font.bold: true
                }

                QQ.Item { Layout.fillWidth: true }

                EditToggleButton {
                    id: settingsEditButton
                    objectName: "regionSettingsEditButton"
                }
            }

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

            // Wrap the GroupBox in a plain Item the outer ColumnLayout manages:
            // a Control placed directly as a Layout child here lands unmanaged
            // at the top and overlaps the rows above it.
            QQ.Item {
                Layout.fillWidth: true
                implicitHeight: coordinateSystemGroupId.implicitHeight

                QC.GroupBox {
                    id: coordinateSystemGroupId
                    objectName: "coordinateSystemGroup"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    title: qsTr("Coordinate system")

                    // The ColumnLayout must BE the contentItem, not a floating
                    // child of it: a Control resizes contentItem.width to
                    // availableWidth, so this is what shrinks the picker down to
                    // the group and lets its controls wrap. A declared child
                    // instead keeps its implicit width and overflows the frame.
                    contentItem: ColumnLayout {
                        spacing: Theme.tightSpacing

                        // Line 1: the bare picker controls (edit mode only). The
                        // name/code lines below present the CS — the project
                        // surface needs no inline resolved label.
                        CSPicker {
                            objectName: "globalCoordinateSystemComboBox"
                            visible: regionInfoBox.editMode
                            Layout.fillWidth: true
                            value: regionInfoBox.csValue
                            allowGeographic: false
                            onCommitted: (newCS) => {
                                RootData.region.geoReference.globalCoordinateSystem = newCS
                            }
                        }

                        // Line 2: friendly name (or "Local" when unset). Hidden for
                        // a CS with no PROJ name — the code line below carries it,
                        // so the raw string never renders on both lines. Copyable.
                        SelectableValue {
                            objectName: "coordinateSystemValue"
                            visible: regionInfoBox.csValue === ""
                                     ? !regionInfoBox.editMode
                                     : CSFormat.hasName(regionInfoBox.csValue)
                            Layout.fillWidth: true
                            text: regionInfoBox.csValue === ""
                                  ? CSFormat.localLabel()
                                  : CSFormat.displayName(regionInfoBox.csValue)
                        }

                        // Line 3: the raw authority code (EPSG:xxxx), hidden for
                        // Local. Copyable — the code is the thing users paste.
                        SelectableValue {
                            objectName: "coordinateSystemCode"
                            visible: regionInfoBox.csValue !== ""
                            Layout.fillWidth: true
                            color: Theme.textSubtle
                            font.family: Theme.fontFamilyMono
                            text: regionInfoBox.csValue
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.delegatePadding

                QC.Label {
                    text: qsTr("Layers:")
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
                Layout.maximumWidth: regionInfoBox.editMode
                                     ? Theme.infoColumnEditMaxWidth
                                     : Theme.infoColumnMaxWidth
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
