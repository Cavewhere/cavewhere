/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSPLAYBUFFER_H
#define CWSPLAYBUFFER_H

//Our includes
#include "CaveWhereLibExport.h"
#include "cwStation.h"
class cwSurveyChunk;

//Qt includes
#include <QHash>
#include <QList>
#include <QString>

/**
 * @brief Collects splays while a survey parses, then hangs them on their stations
 *
 * A station's splays can show up before the leg that introduces the station —
 * TopoDroid writes all of a3's wall shots ahead of the `a0 a4` centerline leg —
 * so an importer buffers them here and attaches them once its chunks are
 * complete.
 *
 * Both importers keep one of these, each at its own lifetime: survex empties it
 * per *begin block, walls per trip.
 */
class CAVEWHERE_LIB_EXPORT cwSplayBuffer
{
public:
    void add(const QString& stationName, const cwShotMeasurement& splay);

    //Returns the stations no chunk reached, sorted by name, so a file always
    //warns about them in the same order
    QList<cwStation> attachTo(const QList<cwSurveyChunk*>& chunks);

    void clear() { m_splays.clear(); }

private:
    //Keyed by cwStation::canonicalKey so "A4" and "a4" collect on the same
    //station, which keeps its as-written name for warnings. Nothing reads this
    //in order — attachTo sorts what it reports, and each station's splays are
    //attached on their own
    QHash<QString, cwStation> m_splays;
};

#endif //CWSPLAYBUFFER_H
