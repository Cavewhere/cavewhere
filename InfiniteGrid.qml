import QtQuick
import CaveWhereSketch
import cavewherelib

Item {
    id: gridId

    anchors.fill: parent

    property double msaaScale: 2.0 //Higher value will cause rendering aliasing
    property double viewScale: 1.0
    property alias viewport: grid1.viewport
    property alias worldToScreenMatrix: grid1.mapMatrix //Holds the map scaling
    property alias gridOrigin: grid1.origin

    function msaaGridVisible(lineWidth) {
        let spacing = 1.0 / Math.sqrt(RootDataSketch.sampleCount * msaaScale);
        let scaledLineWidth = viewScale * lineWidth;
        return scaledLineWidth > spacing;
    }

    FixedGridModel {
        id: grid1
        viewport: containerId.viewport
        // origin: minorGridModel.origin

        lineColor: "#1eb6dd"
        labelColor: "#178ba8"
        labelScale: 1.0 / gridId.viewScale //This keeps the labels visible
        lineWidth: Math.max(1.0, 1.0 * labelScale)


        gridInterval.value: 10.0
        // gridInterval.unit: Units.Feet
        // mapMatrix: worldToScreenId.matrix
        gridVisible: msaaGridVisible(lineWidth)
    }

    FixedGridModel {
        id: grid2
        viewport: grid1.viewport
        origin: grid1.origin
        mapMatrix: grid1.mapMatrix

        lineColor: "#5dcae7"
        lineWidth: Math.max(0.5, 0.5 * grid1.labelScale)

        labelVisible: false
        labelScale: grid1.labelScale

        gridVisible: msaaGridVisible(lineWidth)
        gridInterval.value: 1.0
    }

    ShapePathInstantiator {
        anchors.fill: parent
        model: grid2
    }

    ShapePathInstantiator {
        anchors.fill: parent
        model: grid1
    }

}
