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
    width: 80
    color: "#ffffff"

    //Pageshown is an enumerated type that is either, view, data, draft
    property alias pageShown: buttonBar.currentIndex;

    readonly property string viewPage: "View"
    readonly property string dataPage: "Data"

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
            page = findPage(viewPage);
            break;
        case 1:
            page = findPage(dataPage);
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

            if(address.search(sidebarArea.viewPage) == 0) {
                sidebarArea.pageShown = 0;
            } else if(address.search(sidebarArea.dataPage) == 0) {
                sidebarArea.pageShown = 1
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
                color: "#616469"
            }

            QQ.GradientStop {
                position: 0
                color: "#1b2331"
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
            text: "View"
            image: "qrc:/icons/viewIcon.png"
            troggled: 0 == parent.currentIndex
            onButtonIsTroggled: parent.currentIndex = 0
        }

        SideBarButton {
            id: dataEntyButton
            text: "Data"
            image: "qrc:icons/book.png"
            troggled: 1 == parent.currentIndex
            onButtonIsTroggled: parent.currentIndex = 1           
        }

//        SideBarButton {
//            id: draftButton
//            text: "Draft"
//            troggled: 2 == parent.currentIndex
//            onButtonIsTroggled: parent.currentIndex = 2
//        }
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

            anchors.left: parent.left
            anchors.right: parent.right

            height: columnLayoutId.height + 10

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

        color: "white"

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
        color: "#141414"
    }
}
