import QtQuick as QQ
import QtQuick.Window
import QtCore
// import Qt.labs.settings 1.0

QQ.Item {
    id: itemId

    property Window window;
    property string windowName;

    function resize() {
        function findMinPosition() {
            var point = Qt.point(Infinity, Infinity);

            for(var i in Qt.application.screens) {
                var screen = Qt.application.screens[i];
                point.x = Math.min(screen.virtualX, point.x)
                point.y = Math.min(screen.virtualY, point.y);
            }

            return point;
        }

        var x = settingsId.value("x", window.x);
        var y = settingsId.value("y", window.y);
        var width = settingsId.value("width", window.width);
        var height = settingsId.value("height", window.height);

        //Clamp the width
        var minScreenWidth = itemId.Screen.desktopAvailableWidth;
        var minScreenHeight = itemId.Screen.desktopAvailableHeight;


        width = Math.min(width, minScreenWidth);
        height = Math.min(height, minScreenHeight);

        //Make sure the x is valid
        x += Math.min(0, minScreenWidth - (x + width));
        y += Math.min(0, minScreenHeight - (y + height));

        var minPosition = findMinPosition();
        x = Math.max(minPosition.x, x);
        y = Math.max(minPosition.y, y)

        window.x = x;
        window.y = y;
        window.height = height;
        window.width = width;

        settingsConnectionId.enabled = true
        window.visible = true;
    }

    Settings {
        id: settingsId
        category: itemId.windowName + itemId.Screen.desktopAvailableWidth + "x" +
                  itemId.Screen.desktopAvailableHeight
    }

    QQ.Connections {
        id: settingsConnectionId
        target: itemId.window
        enabled: false
        function onXChanged() {
            settingsId.setValue("x", itemId.window.x)
        }

        function onYChanged() {
            settingsId.setValue("y", itemId.window.y)
        }

        function onHeightChanged() {
            settingsId.setValue("height", itemId.window.height)
        }

        function onWidthChanged() {
            settingsId.setValue("width", itemId.window.width)
        }
    }
}
