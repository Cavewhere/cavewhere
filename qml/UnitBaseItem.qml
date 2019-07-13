import QtQuick 2.0

Item {
    property var unitModel: null
    property int unit
    property bool readOnly: false

    signal newUnit(int unit)
}
