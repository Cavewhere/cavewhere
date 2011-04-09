import Qt 4.7

Rectangle {
    id: rectangle2
    width: 1000
    height: 800

    Rectangle {
        id: sidebarArea
        width: 80
        color: "#ffffff"
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -1
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.top: parent.top
        anchors.topMargin: -1

        Rectangle {
            id: sideBarBackground
            //anchors.left: parent.top
            //anchors.bottom: parent.left
            //anchors.right:  parent.bottom
            //anchors.fill: parent
            //          height: parent.width
            border.width: 0
            border.color: "#000000"
            //            width: parent.height
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

        SideBarButton {
            id: viewButton
            text: "View"
            anchors.top: dataEntyButton.bottom
            anchors.topMargin: 0
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.left: parent.left
            anchors.leftMargin: 0
        }

        SideBarButton {
            id: dataEntyButton
            width: 106
            height: 80
            text: "Data"
            anchors.top: parent.top
            anchors.topMargin: -1
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.left: parent.left
            anchors.leftMargin: 0
        }

        SideBarButton {
            id: drawButton
            text: "Draw"
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.left: parent.left
            anchors.leftMargin: 0
            anchors.top: viewButton.bottom
            anchors.topMargin: -1
        }

        SideBarButton {
            id: draftButton
            text: "Draft"
            anchors.right: parent.right
            anchors.left: parent.left
            anchors.top: drawButton.bottom
            anchors.topMargin: -1
            anchors.rightMargin: 0
            anchors.leftMargin: 0
        }

    }


}
