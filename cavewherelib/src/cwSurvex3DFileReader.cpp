/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvex3DFileReader.h"

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

} // namespace

/**
 * @brief Reads station positions from a survex .3d file
 * @param threeDFilePath - path to the .3d file
 * @return cwStationPositionLookup containing station name -> position mapping
 *
 * This replaces the old pipeline of running survexport to produce a CSV file
 * and then parsing it. The img.h API reads .3d files directly.
 */
cwStationPositionLookup cwSurvex3DFileReader::readStationPositions(const QString& threeDFilePath)
{
    return readNetworkAndLookup(threeDFilePath).lookup;
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
    // cavern emits ':'-joined labels like "cave_<hex>:trip_<hex>:1".
    // cwCavernNaming and every decode consumer assume '.', so normalise the
    // actual separator to '.' at the one point labels enter CaveWhere. The
    // chosen separator is guaranteed never to occur inside a name, so this
    // replacement is unambiguous.
    const QChar separator = QLatin1Char(pimg->separator);

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
            if (separator != QLatin1Char('.')) {
                name.replace(separator, QLatin1Char('.'));
            }
            const QVector3D position(pt.x, pt.y, pt.z);
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
    // above.
    if (!img_rewind(pimg)) {
        qWarning() << "cwSurvex3DFileReader: img_rewind failed for" << threeDFilePath;
        img_close(pimg);
        return out;
    }

    bool hasPrev = false;
    img_point prev{};
    while ((code = img_read_item(pimg, &pt)) != img_STOP) {
        if (code == img_BAD) {
            qWarning() << "cwSurvex3DFileReader: Error reading .3d file (pass 2):" << threeDFilePath;
            break;
        }
        if (code == img_MOVE) {
            prev = pt;
            hasPrev = true;
        } else if (code == img_LINE) {
            if (hasPrev) {
                const auto fromIt = coordToName.constFind(toKey(prev));
                const auto toIt   = coordToName.constFind(toKey(pt));
                if (fromIt != coordToName.constEnd() && toIt != coordToName.constEnd()) {
                    out.network.addShot(*fromIt, *toIt);
                }
            }
            prev = pt;
            hasPrev = true;
        }
    }

    img_close(pimg);
    return out;
}
