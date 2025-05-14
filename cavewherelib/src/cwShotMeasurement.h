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

    // Public member data
    cwDistanceReading distance;
    cwCompassReading  compass;
    cwClinoReading    clino;
    Direction         direction = Direction::Front;
};
