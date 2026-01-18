import QtQuick
import cavewherelib

ListView {
    id: tableViewId

    required property TableStaticColumnModel columnModel

    implicitWidth: columnModel.totalWidth

    clip: true
}
