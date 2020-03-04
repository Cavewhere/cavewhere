import QtQuick 2.12 as QQ

QQ.Text {
    QQ.FontLoader { id: font; source: "qrc:/fonts/YanoneKaffeesatz-Regular.ttf" }

    font.pointSize: 11
    font.family: font.name
}
