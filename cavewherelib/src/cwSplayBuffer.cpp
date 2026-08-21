/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSplayBuffer.h"
#include "cwSurveyChunk.h"

//Qt includes
#include <QHash>

//Std includes
#include <algorithm>

/**
 * @brief cwSplayBuffer::add
 * @param stationName - The named station the splay was shot from
 * @param splay - The wall shot, as a foresight from \a stationName
 */
void cwSplayBuffer::add(const QString& stationName, const cwShotMeasurement& splay)
{
    cwStation& station = m_splays[cwStation::canonicalKey(stationName)];
    if(!station.isValid()) {
        station.setName(stationName);
    }
    station.addSplay(splay);
}

/**
 * @brief cwSplayBuffer::attachTo
 * @param chunks - The chunks the survey parsed into
 * @return The buffered stations that no chunk reached
 *
 * Hangs each station's splays on its first occurrence in \a chunks, which is
 * the one the editor shows, and empties the buffer. The stations that went
 * nowhere come back so the caller can word its own warning.
 */
QList<cwStation> cwSplayBuffer::attachTo(const QList<cwSurveyChunk*>& chunks)
{
    QList<cwStation> skipped;

    if(m_splays.isEmpty()) {
        return skipped;
    }

    //One pass over the chunks, so a survey with splays on every station doesn't
    //rescan them all once per station
    QHash<QString, QPair<cwSurveyChunk*, int>> firstOccurrence;
    for(cwSurveyChunk* chunk : chunks) {
        for(int i = 0; i < chunk->stationCount(); i++) {
            const QString key = cwStation::canonicalKey(chunk->station(i).name());
            if(!key.isEmpty() && !firstOccurrence.contains(key)) {
                firstOccurrence.insert(key, {chunk, i});
            }
        }
    }

    for(auto iter = m_splays.constBegin(); iter != m_splays.constEnd(); ++iter) {
        const auto occurrence = firstOccurrence.constFind(iter.key());
        if(occurrence == firstOccurrence.constEnd()) {
            skipped.append(iter.value());
            continue;
        }

        occurrence->first->setStationSplays(occurrence->second, iter.value().splays());
    }

    m_splays.clear();

    //The buffer hands these back in whatever order it hashed them into, so sort
    //before the caller turns them into warnings — a file that imports twice
    //should read the same both times
    std::sort(skipped.begin(), skipped.end(),
              [](const cwStation& left, const cwStation& right) {
        return cwStation::canonicalKey(left.name()) < cwStation::canonicalKey(right.name());
    });

    return skipped;
}
