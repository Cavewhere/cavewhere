import QtQuick 2.0
import Cavewhere 1.0
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12

StandardPage {

    ColumnLayout {
        Button {
            text: "Resize 1080p"
            onClicked: {
                Window.window.width = 1920
                Window.window.height = 1080
            }
        }
    }
}
