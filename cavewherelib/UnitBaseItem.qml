import QtQuick as QQ

QQ.Item {
    property var unitModel: null
    property int unit
    property bool readOnly: false

    signal newUnit(int unit)
}
