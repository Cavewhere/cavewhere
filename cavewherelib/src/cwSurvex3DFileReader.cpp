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

} // namespace

QPair<QString, QString> cwSurvex3DFileReader::legKey(const QString& from, const QString& to)
{
    const QString first = cwStation::canonicalKey(from);
    const QString second = cwStation::canonicalKey(to);
    if (second < first) {
        return {second, first};
    }
    return {first, second};
}

cwSurvex3DFileReader::NetworkAndLookup cwSurvex3DFileReader::readNetworkAndLookup(const QString& threeDFilePath)
{
    NetworkAndLookup out;

    img* pimg = img_open(threeDFilePath.toUtf8().constData());
    if (!pimg) {
        qWarning() << "cwSurvex3DFileReader: Failed to open .3d file:" << threeDFilePath;
        return out;
    }

    // Pass 1: collect station labels (name + position). Build a coord -> name
    // index so pass 2 can resolve LINE endpoints. Skip anonymous stations —
    // they're never referenced as shot endpoints.
    QHash<CoordKey, QString> coordToName;

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
            QString name = QString::fromUtf8(pimg->label);
            const QVector3D position = toVector3D(pt);
            out.lookup.setPosition(name, position);
            out.network.setPosition(name, position);

            // First label wins at a given coordinate; later duplicates/aliases
            // are ignored for endpoint resolution but still get their own
            // position + network entry.
            const CoordKey key = toKey(pt);
            if (!coordToName.contains(key)) {
                coordToName.insert(key, name);
            }
        }
    }

    // Pass 2: walk MOVE/LINE sequence, resolving shot endpoints by coordinate.
    // img_LINE's pimg->label is the survey-name prefix for the leg, not a
    // station name, so we look up both endpoints in the coord index built
    // above. Splay legs (img_FLAG_SPLAY) have an anonymous end that pass 1
    // skipped, so only their anchor station resolves; the other end is the
    // splay tip.
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
                const auto fromIt = coordToName.constFind(toKey(prev));
                const auto toIt   = coordToName.constFind(toKey(pt));
                const bool fromIsNamed = fromIt != coordToName.constEnd();
                const bool toIsNamed = toIt != coordToName.constEnd();

                if (fromIsNamed && toIsNamed) {
                    // Both ends are real stations, so this is a leg of the
                    // network even when it carries *flags splay.
                    out.network.addShot(*fromIt, *toIt);
                    legsSinceMove.append(legKey(*fromIt, *toIt));
                } else if ((pimg->flags & img_FLAG_SPLAY) && (fromIsNamed || toIsNamed)) {
                    // Exactly one end named: that end is the station the splay
                    // hangs off, and the anonymous end is where it lands.
                    const QString& anchor = fromIsNamed ? *fromIt : *toIt;
                    const img_point& tip = fromIsNamed ? pt : prev;
                    out.splayTips[cwStation::canonicalKey(anchor)].append(toVector3D(tip));
                }
            }
            prev = pt;
            hasPrev = true;
        }
    }

    img_close(pimg);
    return out;
}
