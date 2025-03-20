import QtQuick 2.0
import QtQuick.Layouts 1.1

//Text {
//    z: 5
//    text: "Test!"
//}

Item {
//    width: 400
    height: visible ? layoutId.height - 1 : 0

    RowLayout {
        id: layoutId
        TitleLabel {
            text: "Station"
        }

        TitleLabel {
            text: "Distance"
        }

        TitleLabel {
            text: "Compass"
        }

        TitleLabel {
            text: "Clino"
        }

        TitleLabel {
            text: "L"
        }

        TitleLabel {
            text: "R"
        }

        TitleLabel {
            text: "U"
        }

        TitleLabel {
            text: "D"
        }
    }
}
