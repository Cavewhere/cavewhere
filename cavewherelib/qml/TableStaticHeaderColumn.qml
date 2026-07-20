import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QQ.Rectangle {
    id: delegate

    enum Sort {
        None,
        Ascending,
        Descending
    }

    required property string text;
    required property int columnWidth;

    required property int sortRole; //: -1
    required property var model

    property SortFilterProxyModel _sortModel: model as SortFilterProxyModel

    // Derived from the model rather than stored here: the header delegates
    // are recreated whenever the table is torn down (an empty cave, a
    // narrow/wide flip), and a stored mode would come back cleared while
    // the proxy carried on sorting.
    readonly property int sortMode: {
        if(_sortModel === null || _sortModel.sortRole !== sortRole) {
            return TableStaticHeaderColumn.Sort.None
        }
        return _sortModel.sortOrder === Qt.DescendingOrder
                ? TableStaticHeaderColumn.Sort.Descending
                : TableStaticHeaderColumn.Sort.Ascending
    }

    // Qt6: add cellPadding (and font etc) as public API in headerview
    readonly property real cellPadding: 2

    implicitWidth: columnWidth
    implicitHeight: textId.implicitHeight + (cellPadding * 2)
    color: Theme.surfaceMuted
    border.color: Theme.borderSubtle

    QQ.Item {
        width: delegate.width
        height: delegate.height

        RowLayout {
            anchors.centerIn: parent

            spacing: 1

            QQ.Image {
                id: sortImageId
                visible: delegate.sortMode != TableStaticHeaderColumn.Sort.None
                source: "qrc:/icons/moreArrowDown.png"
                rotation: delegate.sortMode == TableStaticHeaderColumn.Sort.Descending ? 0 : 180
                sourceSize: Qt.size(8, 8)
                smooth: true
            }

            QQ.Item {
                visible: !sortImageId.visible
                implicitWidth: sortImageId.width
            }

            QC.Label {
                id: textId
                text: delegate.text
            }
        }
    }

    QQ.MouseArea {
        anchors.fill: parent
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        onClicked: {
            if(delegate.sortRole >= 0) {
                //Assume that this is a sort proxy mode
                if(delegate._sortModel) {
                    // Only a column already sorted ascending flips to
                    // descending; every other click starts over ascending.
                    let nextOrder = delegate.sortMode === TableStaticHeaderColumn.Sort.Ascending
                            ? Qt.DescendingOrder
                            : Qt.AscendingOrder
                    delegate._sortModel.sortRole = delegate.sortRole
                    delegate._sortModel.sort(nextOrder)
                } else {
                    console.warn("TableStaticHeaderColumn model isn't a cwSortFilterProxyModel, sorting will not work")
                }
            } else {
                console.warn("Delegate has no sortRole")
            }
        }
    }
}
