/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef EMPTYSHOTCHUNKHELPER_H
#define EMPTYSHOTCHUNKHELPER_H

//Our includes
#include "cwError.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

//Qt includes
#include <QStringList>

//Std includes
#include <memory>

namespace EmptyShotChunk {

//A station with every LRUD box filled, so only the shot cells carry errors
inline cwStation station(const QString& name)
{
    const cwDistanceReading lrud(QStringLiteral("1"));
    cwStation result(name);
    result.setLeft(lrud);
    result.setRight(lrud);
    result.setUp(lrud);
    result.setDown(lrud);
    return result;
}

//A complete shot whose backsights agree with its frontsights
inline cwShot readings()
{
    return cwShot(QStringLiteral("10"),
                  QStringLiteral("10"),
                  QStringLiteral("190"),
                  QStringLiteral("0"),
                  QStringLiteral("0"));
}

inline int emptyShotId()
{
    return static_cast<int>(cwErrorTypeId::SurveyEmptyShot);
}

inline QString warningMessage(const QString& from, const QString& to)
{
    return QStringLiteral("Empty shot \"%1\" ➔ \"%2\". Use \"Split chunk here\" if \"%2\" starts a new line")
        .arg(from, to);
}

/**
 * A trip holding one chunk built a shot at a time with appendShot. Each entry
 * of shots pairs with the station it ends on: shots[k] runs from names[k] to
 * names[k + 1].
 */
struct Fixture {
    Fixture(const QStringList& names, const QList<cwShot>& shots)
        : trip(std::make_unique<cwTrip>()),
          chunk(new cwSurveyChunk())
    {
        trip->addChunk(chunk);
        for(int i = 0; i < shots.size(); ++i) {
            chunk->appendShot(station(names.at(i)), station(names.at(i + 1)), shots.at(i));
        }
    }

    //A1 -> KP1 -> (empty) -> KP2 -> B1, the shape from GitHub #721
    static Fixture issue721()
    {
        return Fixture({QStringLiteral("A1"), QStringLiteral("KP1"), QStringLiteral("KP2"), QStringLiteral("B1")},
                       {readings(), cwShot(), readings()});
    }

    std::unique_ptr<cwTrip> trip;
    cwSurveyChunk* chunk;
};

}

#endif // EMPTYSHOTCHUNKHELPER_H
