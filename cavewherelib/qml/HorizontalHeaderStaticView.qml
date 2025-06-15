import QtQuick
import QtQuick.Controls

HorizontalHeaderView {
    id: horizontalHeader
    // Layout.fillWidth: true

    required property TableStaticView view

    model: view.columnModel
    textRole: "name"

    columnWidthProvider: function (column) {
        let columnWidth = explicitColumnWidth(column);
        if(columnWidth == -1) {
            //column width hasn't been set.
            return view.columnModel.get(column).columnWidth;
        }

        return Math.min(300, Math.max(50, columnWidth));
    }

    clip: true

    onLayoutChanged: {
        // Update each ListElement's columnWidth with the current header
        for(let i = 0; i < view.columnModel.count; i++) {
            let column = view.columnModel.get(i);
            if(horizontalHeader.columnWidth(i) != -1
                    && column.columnWidth != horizontalHeader.columnWidth(i))
            {
                column.columnWidth = horizontalHeader.columnWidth(i);
            }
        }
    }
}
