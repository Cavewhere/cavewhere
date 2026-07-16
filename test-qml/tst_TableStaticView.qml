import QtQuick
import QtTest
import cavewherelib

Item {
    id: rootId
    width: 800
    height: 200

    TableStaticColumnModel {
        id: columnModel
        columns: [
            TableStaticColumn {
                id: firstColumn
                columnWidth: 40
                text: "First"
                sortRole: -1
            },
            TableStaticColumn {
                id: secondColumn
                columnWidth: 80
                text: "Second"
                sortRole: -1
            },
            TableStaticColumn {
                id: thirdColumn
                columnWidth: 60
                text: "Third"
                sortRole: -1
            }
        ]
    }

    HorizontalHeaderStaticView {
        id: headerView
        view: tableView
        width: parent.width

        delegate: TableStaticHeaderColumn {
            model: tableView.model
        }
    }

    TableStaticView {
        id: tableView
        y: headerView.height
        width: parent.width
        height: parent.height - headerView.height
        columnModel: columnModel
        model: ListModel {
            ListElement { first: "A"; second: "B"; third: "C" }
        }
        delegate: Item {
            implicitWidth: firstColumn.columnWidth + secondColumn.columnWidth + thirdColumn.columnWidth
            implicitHeight: 20
        }
    }

    TestCase {
        name: "TableStaticView"
        when: windowShown

        function init() {
            firstColumn.columnWidth = 40
            secondColumn.columnWidth = 80
            thirdColumn.columnWidth = 60
        }

        function test_totalWidth() {
            compare(columnModel.totalWidth, 180)
            compare(tableView.implicitWidth, 180)
        }

        function test_columnWidthUpdate() {
            let index = columnModel.index(0, 0)
            columnModel.setData(index, 60, TableStaticColumnModel.ColumnWidthRole)
            compare(firstColumn.columnWidth, 60)
            tryVerify(() => columnModel.totalWidth === 200)
            compare(tableView.implicitWidth, 200)
        }

        function test_headerDelegatesRender() {
            // Reproduces issue #502: in Qt 6.11.1 only the first column
            // header was shown — second was blank, last was "undefined".
            compare(headerView.count, columnModel.count)

            let expected = ["First", "Second", "Third"]
            let widths = [40, 80, 60]

            for (let column = 0; column < expected.length; column++) {
                let delegateItem = headerView.itemAtIndex(column)
                verify(delegateItem !== null,
                    "expected delegate at column " + column)
                compare(delegateItem.text, expected[column],
                    "delegate text at column " + column)
                compare(delegateItem.columnWidth, widths[column],
                    "delegate columnWidth at column " + column)
                compare(delegateItem.width, widths[column],
                    "delegate rendered width at column " + column)
            }
        }
    }
}
