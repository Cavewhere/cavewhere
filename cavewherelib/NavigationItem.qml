import QtQuick 2.0

QtObject {
    property Item item: null
    property var itemFunc: () => { return null; }
    property int indexOffset: 0
}
