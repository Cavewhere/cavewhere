pragma Singleton;
import QtQuick
import cavewherelib

Item {
    // The length model is sourced from cwLengthUnitSelection so the curated set
    // (m, km, ft, mi) lives in exactly one place, shared with the measurement
    // tool's unit selector.
    readonly property list<string> lengthModel: lengthUnitsId.names
    readonly property list<string> depthModel: ["m", "ft"]

    LengthUnitSelection {
        id: lengthUnitsId
    }
}
