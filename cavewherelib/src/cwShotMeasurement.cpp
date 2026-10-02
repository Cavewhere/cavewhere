#include "cwShotMeasurement.h"

//Our includes
#include "cwMath.h"

namespace {

cwCompassReading reversedCompass(const cwCompassReading& compass)
{
    if(compass.state() != cwCompassReading::State::Valid) {
        //A plumbed splay has no bearing, and an unreadable one has no bearing
        //anybody can turn around
        return compass;
    }

    return cwCompassReading(cwWrapDegrees360(compass.toDouble() + cwHalfTurnDegrees));
}

cwClinoReading reversedClino(const cwClinoReading& clino)
{
    switch(clino.state()) {
    case cwClinoReading::State::Up:
        return cwClinoReading(QStringLiteral("down"));
    case cwClinoReading::State::Down:
        return cwClinoReading(QStringLiteral("up"));
    case cwClinoReading::State::Valid:
        return cwClinoReading(-clino.toDouble());
    case cwClinoReading::State::Empty:
    case cwClinoReading::State::Invalid:
        break;
    }
    return clino;
}

}

cwShotMeasurement cwShotMeasurement::reversed() const
{
    return cwShotMeasurement(distance,
                             reversedCompass(compass),
                             reversedClino(clino),
                             direction);
}
