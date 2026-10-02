// cwShotMeasurement.h
#pragma once

#include "cwDistanceReading.h"
#include "cwCompassReading.h"
#include "cwClinoReading.h"

struct cwShotMeasurement {
    // Direction of the shot measurement
    enum class Direction {
        Front,
        Back
    };

    // Default constructor: distance, compass, clino use their own defaults;
    // direction defaults to Front
    cwShotMeasurement() = default;

    // Convenience constructor
    cwShotMeasurement(const cwDistanceReading& distance,
                      const cwCompassReading& compass,
                      const cwClinoReading& clino,
                      Direction direction = Direction::Front)
        : distance(distance)
        , compass(compass)
        , clino(clino)
        , direction(direction)
    {}

    bool operator==(const cwShotMeasurement& other) const = default;

    // This measurement pointing the opposite way: half a turn around from the
    // bearing and the other side of level, so a plumb swaps up for down. A
    // reading that is empty or unreadable comes back untouched, since there's
    // nothing to turn around.
    //
    // Both importers need this for a wall shot a file wrote from the wall point
    // toward the station it hangs on — survex writes that with the anonymous
    // station first, walls with the omitted station first. The direction is
    // left alone: it records which end the instrument was read from, which
    // turning the vector around doesn't change.
    cwShotMeasurement reversed() const;

    // Public member data
    cwDistanceReading distance;
    cwCompassReading  compass;
    cwClinoReading    clino;
    Direction         direction = Direction::Front;
};
