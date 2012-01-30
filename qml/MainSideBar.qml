import QtQuick 1.0

Rectangle {
    id: sidebarArea
    width: 80
    color: "#ffffff"

    //Pageshown is an enumerated type that is either, view, data, draft
    property alias pageShown: buttonBar.currentIndex;


    Rectangle {
        id: sideBarBackground
        border.width: 0
       // border.color: "#000000"
        height: parent.width
        gradient: Gradient {
            GradientStop {
                position: 1
                color: "#616469"
            }

            GradientStop {
                position: 0
                color: "#1b2331"
            }
        }
        width: parent.height
        x: -parent.height / 2
        y: parent.height / 2
        rotation: -90
        transformOrigin: Item.Top

    }



    Column {
        id: buttonBar
        anchors.fill: parent

        property int currentIndex: 1

        SideBarButton {
            id: viewButton
            text: "View"
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

        SideBarButton {
            id: draftButton
            text: "Draft"
            troggled: 2 == parent.currentIndex
            onButtonIsTroggled: parent.currentIndex = 2
        }
    }
}
