/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

import QtQuick 2.0 as QQ
import QtQuick.Layouts 1.1

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
        var history = rootData.pageSelectionModel.history;
        for(var i = history.length - 1; i >= 0; i--) {
            var page = history[i];
            var fullAddress = page.fullname();
            if(fullAddress.search(pageType) === 0) {
                return fullAddress;
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

        rootData.pageSelectionModel.currentPageAddress = page;
    }

    QQ.Connections {
        target: rootData.pageSelectionModel

        onCurrentPageAddressChanged: {
            gotoToPage = false;
            var address = rootData.pageSelectionModel.currentPageAddress;

            if(address.search(viewPage) == 0) {
                pageShown = 0;
            } else if(address.search(dataPage) == 0) {
                pageShown = 1
            }
            gotoToPage = true
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
        anchors.bottom: parent.bottom

        model: rootData.taskManagerModel
        verticalLayoutDirection: QQ.ListView.BottomToTop

        delegate: QQ.Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right

            height: columnLayoutId.height + 10

            ColumnLayout {
                id: columnLayoutId

                y: 5

                anchors.left: parent.left
                anchors.right: parent.right

                Text {
                    id: nameText
                    text: nameRole
                }

                QQ.Rectangle {
                    border.width: 1
                    border.color: "black"

                    Layout.fillWidth: true

                    implicitHeight: 12

                    QQ.Rectangle {
                        property double precentComplete: progressRole / numberOfStepsRole
                        x: 1
                        y: 1
                        width: (parent.width - 2) * precentComplete
                        height: parent.height - 2
                        color: "red"
                    }
                }
            }
        }
    }
}
