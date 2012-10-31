import QtQuick 2.0
import Cavewhere 1.0

Column {
    property Cave currentCave: null

    CaveLengthComponent {
        id: caveLengthId
        text: "Length:"
        unitValue: currentCave !== null ? currentCave.length : null
        unitModel: ["m", "km", "ft", "mi"]
    }

    CaveLengthComponent {
        id: caveDepth
        text: "Depth:"
        unitValue: currentCave !== null ? currentCave.depth : null
        unitModel: ["m", "ft"]
    }

}
