/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QQ.Rectangle {
    id: sidebarArea
    objectName: "mainSideBar"
    width: 80
    color: Theme.sidebar.background

    //Pageshown is an enumerated type that is either, view, data, draft
    property alias pageShown: buttonBar.currentIndex;

    property Page viewPage;
    property Page dataPage;
    property Page mapPage;

    readonly property string _viewPage: viewPage.fullname()
    readonly property string _dataPage: dataPage.fullname()
    readonly property string _mapPage: mapPage.fullname()

    property bool gotoToPage: true

    /**
      pageType should be either "View" or "Data"
      */
    function findPage(pageType) {
        var history = RootData.pageSelectionModel.history;
        for(var i = history.length - 1; i >= 0; i--) {
            var page = history[i];
            if(page) {
                var fullAddress = page.fullname();
                if(fullAddress.search(pageType) === 0) {
                    return fullAddress;
                }
            }
        }
        return pageType;
    }

    //This is for global page selection
    onPageShownChanged: {
        if(!gotoToPage) { return; }

        var page;
        switch(pageShown) {
        case 0:
            page = findPage(_viewPage);
            break;
        case 1:
            page = findPage(_dataPage);
            break;
        case 2:
            page = findPage(_mapPage);
            break;
        default:
            console.log("Don't know how to show page:" + pageShown);
        }

        RootData.pageSelectionModel.currentPageAddress = page;
    }

    QQ.Connections {
        target: RootData.pageSelectionModel

        function onCurrentPageAddressChanged() {
            sidebarArea.gotoToPage = false;
            var address = RootData.pageSelectionModel.currentPageAddress;

            if(address.search(sidebarArea._viewPage) == 0) {
                sidebarArea.pageShown = 0;
            } else if(address.search(sidebarArea._dataPage) == 0) {
                sidebarArea.pageShown = 1
            } else if(address.search(sidebarArea._mapPage) == 0) {
                sidebarArea.pageShown = 2
            } else {
                //Deselect both, probably in unknown page or settings page
                sidebarArea.pageShown = -1;
            }

            sidebarArea.gotoToPage = true
        }
    }

    QQ.Rectangle {
        id: sideBarBackground
        border.width: 0
       // border.color: "#000000"
        height: parent.width
        gradient: QQ.Gradient {
            QQ.GradientStop {
                position: 1
                color: Theme.sidebar.gradientBottom
            }

            QQ.GradientStop {
                position: 0
                color: Theme.sidebar.gradientTop
            }
        }
        width: parent.height
        x: -parent.height / 2
        y: parent.height / 2
        rotation: -90
        transformOrigin: QQ.Item.Top

    }



    QQ.Column {
        id: buttonBar
        anchors.left: parent.left
        anchors.right: parent.right

        property int currentIndex: 0

        SideBarButton {
            id: viewButton
            objectName: "viewButton"
            text: "View"
            image: "qrc:/twbs-icons/icons/box.svg"
            troggled: 0 == buttonBar.currentIndex
            onButtonIsTroggled: buttonBar.currentIndex = 0
        }

        SideBarButton {
            id: dataEntyButton
            objectName: "dataButton"
            text: "Data"
            image: "qrc:/twbs-icons/icons/book.svg"
            troggled: 1 == buttonBar.currentIndex
            onButtonIsTroggled: buttonBar.currentIndex = 1
        }

        SideBarButton {
            id: mapButton
            objectName: "mapButton"
            text: "Map"
            image: "qrc:/twbs-icons/icons/map.svg"
            troggled: 2 == buttonBar.currentIndex
            onButtonIsTroggled: buttonBar.currentIndex = 2
        }
    }

    QQ.ListView {
        id: taskListView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: buttonBar.bottom
        anchors.bottom: autoSwitchId.top

        TaskFutureCombineModel {
            id: taskModelCombinerId
            models: [RootData.taskManagerModel, RootData.futureManagerModel]
        }

        FutureFilterModel {
            id: futureFilterId
            sourceModel: taskModelCombinerId
        }

        model: taskModelCombinerId
        verticalLayoutDirection: QQ.ListView.BottomToTop

        delegate: QQ.Rectangle {
            id: delegateId
            required property string nameRole
            required property int progressRole
            required property int numberOfStepsRole

            anchors.left: parent ? parent.left : undefined
            anchors.right: parent ? parent.right : undefined

            height: columnLayoutId.height + 10

            color: Theme.background

            ColumnLayout {
                id: columnLayoutId

                y: 5

                anchors.margins: 5
                anchors.left: parent.left
                anchors.right: parent.right

                Text {
                    id: nameText
                    text: delegateId.nameRole
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                QC.ProgressBar {
                    Layout.maximumWidth: columnLayoutId.width
                    value: !indeterminate ? delegateId.progressRole / delegateId.numberOfStepsRole : 0.0
                    indeterminate: delegateId.numberOfStepsRole <= 0
                }
            }
        }
    }

    QQ.Rectangle {
        id: autoSwitchId
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        implicitHeight: autoSwitchLayoutId.height

        color: Theme.sidebar.panel

        ColumnLayout {
            id: autoSwitchLayoutId
            anchors.left: parent.left
            anchors.right: parent.right

            Text {
                text: "Automatic\nUpdate"
                id: labelTextId
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            QC.CheckBox {
                id: autoCheckboxId
                checked: RootData.settings.jobSettings.automaticUpdate
                onCheckedChanged: {
                    RootData.settings.jobSettings.automaticUpdate = checked
                }
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }

    QQ.Rectangle {
        id: verticalLine
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.sidebar.divider
    }
}
