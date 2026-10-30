/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvex3DFileReader.h"
#include "cwStation.h"

//Survex library
#include "img.h"

//Qt includes
#include <QVector3D>
#include <QHash>
#include <QDebug>

#include <algorithm>
#include <cmath>

namespace {

// Coordinate-key precision for coord -> station-name index. Survex writes
// positions in metres; 4 decimal places (= 0.1 mm) is well below any real-world
// station spacing and keeps us safely away from any floating-point equality
// pitfalls.
constexpr double kCoordKeyScale = 10000.0;

struct CoordKey {
    qint64 x;
    qint64 y;
    qint64 z;

    bool operator==(const CoordKey& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

size_t qHash(const CoordKey& key, size_t seed = 0) noexcept {
    return qHashMulti(seed, key.x, key.y, key.z);
}

CoordKey toKey(const img_point& p) {
    return {
        static_cast<qint64>(std::llround(p.x * kCoordKeyScale)),
        static_cast<qint64>(std::llround(p.y * kCoordKeyScale)),
        static_cast<qint64>(std::llround(p.z * kCoordKeyScale))
    };
}

QVector3D toVector3D(const img_point& p) {
    return QVector3D(p.x, p.y, p.z);
}

QString normalizedLabel(QString name, QChar separator)
{
    if (separator != QLatin1Char('.')) {
        name.replace(separator, QLatin1Char('.'));
    }
    return name;
}

QString normalizedLabel(const char* label, QChar separator)
{
    return normalizedLabel(QString::fromUtf8(label), separator);
}

// Picks which of the labels at one coordinate names a leg's end. An equate or
// a coincidence puts several stations at one point; the leg belongs to the
// survey cavern labels it with, which is the parent survey of one of its ends.
// The station whose own survey shares the longest leading run with that label
// wins, then the one whose survey sits closest to it, then the first label.
QString legEndName(const QStringList& candidates, const QStringList& legSurvey)
{
    if (candidates.size() == 1) {
        return candidates.first();
    }

    QString best = candidates.first();
    qsizetype bestShared = 0;
    qsizetype bestExtra = 0;
    for (const QString& candidate : candidates) {
        QStringList survey = candidate.split(QLatin1Char('.'));
        survey.removeLast();

        qsizetype shared = 0;
        const qsizetype limit = qMin(survey.size(), legSurvey.size());
        while (shared < limit
               && survey.at(shared).compare(legSurvey.at(shared), Qt::CaseInsensitive) == 0) {
            ++shared;
        }
        const qsizetype extra = survey.size() - shared;

        const bool longerMatch = shared > bestShared;
        const bool closerMatch = shared > 0 && shared == bestShared && extra < bestExtra;
        if (longerMatch || closerMatch) {
            best = candidate;
            bestShared = shared;
            bestExtra = extra;
        }
    }
    return best;
}

struct Label {
    QString name;
    QStringList surveyLevels;
    QString station;
    CoordKey coord;
    QVector3D position;
    bool fixed;
    bool entrance;
};

Label readLabel(const img* pimg, const img_point& pt, QChar separator)
{
    const QString raw = QString::fromUtf8(pimg->label);
    QStringList levels = raw.split(separator);
    const QString station = levels.takeLast();
    return {normalizedLabel(raw, separator),
            levels,
            station,
            toKey(pt),
            toVector3D(pt),
            (pimg->flags & img_SFLAG_FIXED) != 0,
            (pimg->flags & img_SFLAG_ENTRANCE) != 0};
}

// The label that names a group of labels at one coordinate: its all-lowercase
// spelling when it has one, otherwise the first label in .3d order.
const Label& representative(const QList<Label>& labels, const QList<qsizetype>& indexes)
{
    for (const qsizetype index : indexes) {
        const Label& label = labels.at(index);
        if (label.station == cwStation::canonicalKey(label.station)) {
            return label;
        }
    }
    return labels.at(indexes.first());
}

QString spellingOf(const Label& label)
{
    return cwStation::caseDistinctKey(label.surveyLevels, label.station);
}

} // namespace

QPair<QString, QString> cwSurvex3DFileReader::legKey(const QString& from, const QString& to)
{
    return legKeyOfKeys(cwStation::canonicalKey(from), cwStation::canonicalKey(to));
}

QPair<QString, QString> cwSurvex3DFileReader::legKeyOfKeys(const QString& fromKey, const QString& toKey)
{
    if (toKey < fromKey) {
        return {toKey, fromKey};
    }
    return {fromKey, toKey};
}

cwSurvex3DFileReader::NetworkAndLookup cwSurvex3DFileReader::readNetworkAndLookup(const QString& threeDFilePath)
{
    NetworkAndLookup out;

    img* pimg = img_open(threeDFilePath.toUtf8().constData());
    if (!pimg) {
        qWarning() << "cwSurvex3DFileReader: Failed to open .3d file:" << threeDFilePath;
        return out;
    }

    // Survex chooses the label separator dynamically for the whole .3d
    // (find_output_separator in survex/src/commands.c): it is '.' only when '.'
    // is never a name character. Processing any Walls (.srv) or Compass (.dat)
    // data in the run — which a region solve may include alongside native
    // survex caves — marks '.' as a name character and ':' as a separator, so
    // cavern emits ':'-joined labels like "fisher_ridge:topo1:1".
    // cwCavernNaming and every decode consumer assume '.', so normalize the
    // actual separator to '.' at the one point labels enter CaveWhere.
    //
    // The choice is made once, before any Compass station is read, so a
    // Compass-only run keeps '.' even where a station name spells one
    // (measured on compass_dotted.dat: "cave0.SA1.1" is the wrapper plus the
    // one station "SA1.1"). The replacement stays unambiguous only because a
    // cave-level or trip-level wrapper is the sole level above such a name —
    // see "Separator ambiguity" filed in plans/EXTERNAL_FILE_PHASE3.html for
    // the level that would break it.
    const QChar separator = QLatin1Char(pimg->separator);

    // Pass 1: collect station labels (name + position). Build a coord -> names
    // index so pass 2 can resolve LINE endpoints. Skip anonymous stations —
    // they're never referenced as shot endpoints.
    QHash<CoordKey, QStringList> coordToNames;
    QList<Label> labels;

    img_point pt;
    int code;
    while ((code = img_read_item(pimg, &pt)) != img_STOP) {
        if (code == img_BAD) {
            qWarning() << "cwSurvex3DFileReader: Error reading .3d file (pass 1):" << threeDFilePath;
            img_close(pimg);
            return NetworkAndLookup{};
        }
        if (code == img_LABEL) {
            if (pimg->flags & img_SFLAG_ANON) {
                continue;
            }
            labels.append(readLabel(pimg, pt, separator));

            // Equated or coincident stations share a coordinate; pass 2 picks
            // among them by the leg's survey.
            coordToNames[labels.last().coord].append(labels.last().name);
        }
    }

    // Cavern folds case only where the data asks it to (a Compass or Walls
    // file, or *case preserve), so P2 and p2 can be two stations. Labels that
    // lowercase alike are one station when they share a coordinate (an equate
    // or a coincidence) and keep the lowercase key. At different coordinates
    // they are a case pair, and each member keys by its own spelling.
    QHash<QString, QList<qsizetype>> labelsByCanonical;
    labelsByCanonical.reserve(labels.size());
    for (qsizetype i = 0; i < labels.size(); ++i) {
        labelsByCanonical[cwStation::canonicalKey(labels.at(i).name)].append(i);
    }

    QHash<QString, QString> keyOfName;
    keyOfName.reserve(labels.size());
    const auto addStation = [&](const QList<qsizetype>& indexes, bool isCasePair) {
        const Label& named = representative(labels, indexes);
        const QString key = isCasePair ? spellingOf(named) : cwStation::canonicalKey(named.name);
        out.lookup.setPositionAtKey(key, named.position);
        out.network.setPositionAtKey(key, named.position);
        out.spellings.insert(key, spellingOf(named));
        out.labels.insert(key, named.name);
        for (const qsizetype index : indexes) {
            keyOfName.insert(labels.at(index).name, key);
        }
    };

    for (auto group = labelsByCanonical.cbegin(); group != labelsByCanonical.cend(); ++group) {
        const QList<qsizetype>& indexes = group.value();

        // Groups are a handful of labels, so a linear scan per coordinate stays cheap.
        QList<QList<qsizetype>> byCoord;
        for (const qsizetype index : indexes) {
            const CoordKey& coord = labels.at(index).coord;
            const auto sameCoord = std::find_if(byCoord.begin(), byCoord.end(),
                                                [&](const QList<qsizetype>& atCoord) {
                return labels.at(atCoord.first()).coord == coord;
            });
            if (sameCoord == byCoord.end()) {
                byCoord.append({index});
            } else {
                sameCoord->append(index);
            }
        }

        const bool isCasePair = byCoord.size() > 1;
        for (const QList<qsizetype>& atCoord : std::as_const(byCoord)) {
            addStation(atCoord, isCasePair);
        }
    }

    for (const Label& label : std::as_const(labels)) {
        if (label.fixed) {
            out.fixedStations.append(keyOfName.value(label.name));
        }
        if (label.entrance) {
            out.entranceStations.append(label.name);
        }
    }

    // Pass 2: walk MOVE/LINE sequence, resolving shot endpoints by coordinate.
    // img_LINE's pimg->label is the full survey prefix of the leg, not a
    // station name, so we look up both endpoints in the coord index built
    // above and name each end under that survey. Splay legs (img_FLAG_SPLAY)
    // have an anonymous end that pass 1 skipped, so only their anchor station
    // resolves; the other end is the splay tip.
    if (!img_rewind(pimg)) {
        qWarning() << "cwSurvex3DFileReader: img_rewind failed for" << threeDFilePath;
        img_close(pimg);
        return out;
    }

    bool hasPrev = false;
    img_point prev{};
    // Named legs since the last MOVE: cavern writes a traverse's
    // img_ERROR_INFO right after its LINEs, and the record covers the last
    // n_legs of them (aven's rule, survex/src/model.cc).
    QList<QPair<QString, QString>> legsSinceMove;
    while ((code = img_read_item(pimg, &pt)) != img_STOP) {
        if (code == img_BAD) {
            qWarning() << "cwSurvex3DFileReader: Error reading .3d file (pass 2):" << threeDFilePath;
            break;
        }
        if (code == img_MOVE) {
            prev = pt;
            hasPrev = true;
            legsSinceMove.clear();
        } else if (code == img_ERROR_INFO) {
            // Cavern misses some articulating traverses and writes them with
            // all-zero error; aven treats those as outside any loop, and so
            // do we.
            const bool hasError = pimg->E != 0.0 || pimg->H != 0.0 || pimg->V != 0.0;
            if (hasError) {
                const cwLegError error{float(pimg->E), float(pimg->H), float(pimg->V)};
                const qsizetype legCount = qMin<qsizetype>(pimg->n_legs, legsSinceMove.size());
                for (qsizetype i = legsSinceMove.size() - legCount; i < legsSinceMove.size(); ++i) {
                    out.legErrors.insert(legsSinceMove.at(i), error);
                }
            }
        } else if (code == img_LINE) {
            if (hasPrev) {
                const auto fromIt = coordToNames.constFind(toKey(prev));
                const auto toIt   = coordToNames.constFind(toKey(pt));
                const bool fromIsNamed = fromIt != coordToNames.constEnd();
                const bool toIsNamed = toIt != coordToNames.constEnd();

                QStringList legSurvey;
                const auto endName = [&](const QStringList& candidates) {
                    if (candidates.size() > 1 && legSurvey.isEmpty()) {
                        legSurvey = normalizedLabel(pimg->label, separator)
                                        .split(QLatin1Char('.'), Qt::SkipEmptyParts);
                    }
                    return legEndName(candidates, legSurvey);
                };

                if (fromIsNamed && toIsNamed) {
                    // Both ends are real stations, so this is a leg of the
                    // network even when it carries *flags splay.
                    const QString fromStation = keyOfName.value(endName(*fromIt));
                    const QString toStation = keyOfName.value(endName(*toIt));
                    out.network.addShotKeys(fromStation, toStation);
                    legsSinceMove.append(legKeyOfKeys(fromStation, toStation));
                } else if ((pimg->flags & img_FLAG_SPLAY) && (fromIsNamed || toIsNamed)) {
                    // Exactly one end named: that end is the station the splay
                    // hangs off, and the anonymous end is where it lands.
                    const QString anchor = endName(fromIsNamed ? *fromIt : *toIt);
                    const img_point& tip = fromIsNamed ? pt : prev;
                    out.splayTips[keyOfName.value(anchor)].append(toVector3D(tip));
                }
            }
            prev = pt;
            hasPrev = true;
        }
    }

    img_close(pimg);
    return out;
}
