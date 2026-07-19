import QtQuick.Layouts
import QtQuick.Controls as QC
import cavewherelib
import "Utils.js" as Utils

RowLayout {
    id: rootId

    property string label
    property UnitValue unitValue: null
    // A length's unit follows its magnitude (m/km, ft/mi); a depth always uses
    // the small unit (m/ft). Set true for the depth stat.
    property bool depth: false

    // The unit is resolved live from the project's unit system, never from the
    // cave's stored length/depth unit (which stays metres regardless).
    readonly property int caveUnitSystem: ProjectUnits.unitSystem
    readonly property real meters: rootId.unitValue !== null
        ? Units.convertLength(rootId.unitValue.value, rootId.unitValue.unit, Units.Meters)
        : 0.0
    readonly property int displayUnit: rootId.depth
        ? Units.depthDisplayUnit(rootId.caveUnitSystem)
        : Units.lengthDisplayUnit(rootId.meters, rootId.caveUnitSystem)

    spacing: Theme.delegatePadding

    QC.Label {
        text: rootId.label
        // An empty label (the inline cave-list use) must not claim a RowLayout
        // spacing slot, which would push the value off from the sentence.
        visible: rootId.label !== ""
    }

    SelectableValue {
        text: rootId.unitValue !== null
            ? Utils.fixed(Units.convertLength(rootId.meters, Units.Meters, rootId.displayUnit), 2)
            : ""
        font.pixelSize: Theme.fontSizeUI
    }

    QC.Label {
        text: rootId.unitValue !== null ? Units.lengthUnitName(rootId.displayUnit) : ""
    }
}
