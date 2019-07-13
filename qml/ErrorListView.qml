import QtQuick 2.0
import QtQuick.Layouts 1.0
import QtQuick.Controls 2.5
import Cavewhere 1.0

ScrollView {
    property alias model: listViewId.model

    ListView {
        id: listViewId

        implicitHeight: 300
        implicitWidth: 600

        delegate: RowLayout {
            Image {
                source: {
                    switch(type) {
                    case CwError.Warning:
                        return "qrc:icons/warning.png"
                    case CwError.Fatal:
                        return "qrc:icons/stopSignError.png"
                    case CwError.NoError:
                        return "qrc:icons/good.png"
                    }
                }
            }

            Text {
                text: message
            }
        }
    }
}
