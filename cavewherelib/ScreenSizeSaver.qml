import QtQuick 2.10 as QQ
import QtQuick.Window 2.0
import Qt.labs.settings 1.0

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
        var minScreenWidth = window.screen.desktopAvailableWidth; //Math.min(window.screen.width, window.screen.desktopAvailableWidth);
        var minScreenHeight = window.screen.desktopAvailableHeight; //Math.min(window.screen.height, window.screen.desktopAvailableHeight);

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
        category: windowName + window.screen.desktopAvailableWidth + "x" +
                  window.screen.desktopAvailableHeight
    }

    QQ.Connections {
        id: settingsConnectionId
        target: window
        enabled: false
        onXChanged: {
            settingsId.setValue("x", window.x)
        }

        onYChanged: {
            settingsId.setValue("y", window.y)
        }

        onHeightChanged: {
            settingsId.setValue("height", window.height)
        }

        onWidthChanged: {
            settingsId.setValue("width", window.width)
        }
    }
}
