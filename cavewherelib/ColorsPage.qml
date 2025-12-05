import QtQuick as QQ
import QtQuick.Controls as QC
import QtQuick.Layouts
import cavewherelib

QC.ScrollView {
    id: root

    anchors.margins: 5

    // Reusable grid for color swatches
    component ColorDelegate: QQ.Item {
        width: rectId.width
        height: layoutId.height

        ColumnLayout {
            id: layoutId

            anchors {
                left: parent.left
                right: parent.right
                margins: 4
            }

            spacing: 2

            QQ.Rectangle {
                id: rectId
                width: 100
                height: 100
                radius: 6
                color: model.color
                border.width: 1
                border.color: Theme.border

                Text {
                    anchors.centerIn: parent
                    text: "Hello world!"
                }
            }


            Text {
                text: model.name
                font.bold: true
                color: Theme.text
                wrapMode: Text.Wrap
            }

            Text {
                text: model.color
                color: Theme.text
                wrapMode: Text.Wrap
            }
        }

    }

    component ColorGrid: ColumnLayout {
        property alias title: titleLabel.text
        property alias model: repeater.model

        spacing: 6

        Text {
            id: titleLabel
            font.pixelSize: 18
            font.bold: true
            color: Theme.text
            Layout.leftMargin: 4
        }

        QQ.Flow {
            Layout.fillWidth: true
            spacing: 12
            QQ.Repeater {
                id: repeater
                delegate: ColorDelegate {}
            }
        }
    }


    ColumnLayout {
        id: layout
        width: Math.max(800, root.width - 20)
        spacing: 16
        anchors.margins: 12
        property int sortIndex: 0 // 0 = name, 1 = color

        function refreshModels() {
            paletteModel.clear()
            themeModel.clear()
            sidebarModel.clear()
            populateModel(Theme.palette, paletteModel)
            populateModel(Theme, themeModel, ["palette", "sidebar"])
            populateModel(Theme.sidebar, sidebarModel)
            sortAll()
        }

        QQ.Component.onCompleted: refreshModels()

        QQ.Connections {
            target: Theme
            function onDarkChanged() { layout.refreshModels() }
        }

        function populateModel(source, model, skipKeys) {
            if (!source || !model) {
                return;
            }
            for (var key in source) {
                if (skipKeys && skipKeys.indexOf(key) !== -1) {
                    continue;
                }
                var value = source[key];
                var valueStr = value !== undefined && value !== null && value.toString ? value.toString() : "";
                // Heuristic: treat strings that look like colors as colors.
                if (typeof valueStr === "string" && valueStr.length > 0) {
                    var lower = valueStr.toLowerCase();
                    var looksLikeColor = lower[0] === "#" || lower.indexOf("rgb") === 0 || lower === "transparent" || lower.match(/^[a-z]+$/);
                    if (looksLikeColor) {
                        model.append({ name: key, color: valueStr });
                    }
                }
            }
        }

        Text {
            text: "Color Scheme is: " + (Theme.dark ? "Dark" : "Light")
            font.pixelSize: 20
            font.bold: true
        }

        RowLayout {
            spacing: 8
            Text {
                text: "Sort by:"
                color: Theme.text
            }
            QC.ComboBox {
                id: sortBox
                model: ["Name", "Color"]
                currentIndex: layout.sortIndex
                onActivated: {
                    layout.sortIndex = currentIndex
                    layout.sortAll()
                }
            }
        }

        QQ.ListModel { id: paletteModel }
        QQ.ListModel { id: themeModel }
        QQ.ListModel { id: sidebarModel }

        function sortModelBy(model, key) {
            if (!model || model.count === 0) return;
            let items = [];
            for (let i = 0; i < model.count; ++i) {
                let item = model.get(i)
                items.push({name: item.name, color: item.color});
            }
            items.sort(function(a, b) {
                const av = (a[key] || "").toString().toLowerCase();
                const bv = (b[key] || "").toString().toLowerCase();
                if (av < bv) return -1;
                if (av > bv) return 1;
                return 0;
            });
            model.clear();
            for (const it of items) {
                model.append(it);
            }
        }

        function sortAll() {
            const key = layout.sortIndex === 0 ? "name" : "color";
            sortModelBy(paletteModel, key);
            sortModelBy(themeModel, key);
            sortModelBy(sidebarModel, key);
        }

        ColorGrid {
            title: "Palette (Theme.palette)"
            model: paletteModel
        }

        ColorGrid {
            title: "Theme Colors"
            model: themeModel
        }

        ColorGrid {
            title: "Theme.sidebar"
            model: sidebarModel
        }
    }
}
