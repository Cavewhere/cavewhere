#pragma once

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"

//Qt includes
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVector3D>

//Std includes
#include <algorithm>
#include <string>

//! compass_case_pairs.dat: P1/p1, P2/p2 and P3/p3 are three case pairs whose
//! members sit 100 ft apart, joined by six shots; Q1 pairs with nothing.
namespace casePairs {

inline constexpr double kSeparationMeters = 30.48;
// Solved positions are floats, so a drawn leg drifts by fractions of a
// centimeter off its tape length.
inline constexpr double kLegMarginMeters = 0.05;
// Vertices of one drawn segment match the solved positions to well below a
// millimeter.
inline constexpr float kSamePointMeters = 0.001f;
inline constexpr int kShotCount = 6;

inline const QString kFileName = QStringLiteral("compass_case_pairs.dat");
inline const QString kMakFileName = QStringLiteral("compass_case_pairs.mak");

//! The solved keys of the file's seven stations under \a prefix.
inline QStringList keys(const QString& prefix)
{
    QStringList keys;
    for (const QString& station : {QStringLiteral("P1"), QStringLiteral("P2"), QStringLiteral("P3"),
                                   QStringLiteral("p1"), QStringLiteral("p2"), QStringLiteral("p3"),
                                   QStringLiteral("q1")}) {
        keys.append(prefix + station);
    }
    return keys;
}

//! The file's six shots as solved keys under \a prefix.
inline QList<QPair<QString, QString>> shots(const QString& prefix)
{
    const auto shot = [&prefix](const QString& from, const QString& to) {
        return qMakePair(prefix + from, prefix + to);
    };
    return {shot(QStringLiteral("P1"), QStringLiteral("P2")),
            shot(QStringLiteral("P2"), QStringLiteral("P3")),
            shot(QStringLiteral("P3"), QStringLiteral("q1")),
            shot(QStringLiteral("P1"), QStringLiteral("p1")),
            shot(QStringLiteral("p1"), QStringLiteral("p2")),
            shot(QStringLiteral("p2"), QStringLiteral("p3"))};
}

//! \a list sorted and comma-joined, so a failure prints it.
inline std::string joined(QStringList list)
{
    list.sort();
    return list.join(QStringLiteral(", ")).toStdString();
}

//! The keys \a lookup stores, as joined() prints them.
inline std::string storedKeys(const cwStationPositionLookup& lookup)
{
    return joined(lookup.positions().keys());
}

//! Checks that each pair member under \a prefix is its own station: its own
//! stored position, 100 ft from its partner, and its own neighbors.
inline void checkStations(const cwStationPositionLookup& lookup, const cwSurveyNetwork& network,
                          const QString& prefix)
{
    INFO("stored keys: " << storedKeys(lookup));
    const QMap<QString, QVector3D> positions = lookup.positions();
    for (const QString& key : keys(prefix)) {
        INFO("key " << key.toStdString());
        CHECK(positions.contains(key));
    }
    for (const QString& station : {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3")}) {
        const QString upper = prefix + QStringLiteral("P") + station;
        const QString lower = prefix + QStringLiteral("p") + station;
        INFO("pair " << upper.toStdString() << " / " << lower.toStdString());
        CHECK(positions.value(upper).distanceToPoint(positions.value(lower))
              == Catch::Approx(kSeparationMeters).margin(kLegMarginMeters));
    }

    const auto neighborsOf = [&network](const QString& key) { return joined(network.neighbors(key)); };
    CHECK(neighborsOf(prefix + QStringLiteral("P2"))
          == joined({prefix + QStringLiteral("P1"), prefix + QStringLiteral("P3")}));
    CHECK(neighborsOf(prefix + QStringLiteral("p2"))
          == joined({prefix + QStringLiteral("p1"), prefix + QStringLiteral("p3")}));
    CHECK(neighborsOf(prefix + QStringLiteral("P1"))
          == joined({prefix + QStringLiteral("P2"), prefix + QStringLiteral("p1")}));
    CHECK(neighborsOf(prefix + QStringLiteral("q1")) == joined({prefix + QStringLiteral("P3")}));
}

inline bool samePoint(const QVector3D& a, const QVector3D& b)
{
    return a.distanceToPoint(b) < kSamePointMeters;
}

//! Checks that \a points, a line list holding only the file's legs, draws
//! each of its six shots once between the stored positions of the shot's two
//! stations under \a prefix, and no leg longer than the pairs' 100 ft.
inline void checkDrawsShots(const QVector<QVector3D>& points, const cwStationPositionLookup& lookup,
                            const QString& prefix)
{
    INFO("stored keys: " << storedKeys(lookup));
    const QMap<QString, QVector3D> positions = lookup.positions();
    REQUIRE(points.size() == 2 * kShotCount);

    QList<QPair<QString, QString>> undrawn = shots(prefix);
    for (qsizetype i = 0; i + 1 < points.size(); i += 2) {
        const QVector3D from = points.at(i);
        const QVector3D to = points.at(i + 1);
        INFO("segment " << i / 2 << " is " << from.distanceToPoint(to) << " m long");
        CHECK(from.distanceToPoint(to) <= kSeparationMeters + kLegMarginMeters);

        const auto drawn = std::find_if(undrawn.begin(), undrawn.end(),
                                        [&](const QPair<QString, QString>& shot) {
            const auto fromIt = positions.constFind(shot.first);
            const auto toIt = positions.constFind(shot.second);
            if (fromIt == positions.constEnd() || toIt == positions.constEnd()) {
                return false;
            }
            return (samePoint(from, fromIt.value()) && samePoint(to, toIt.value()))
                   || (samePoint(from, toIt.value()) && samePoint(to, fromIt.value()));
        });
        CHECK(drawn != undrawn.end());
        if (drawn != undrawn.end()) {
            undrawn.erase(drawn);
        }
    }

    QStringList missing;
    for (const auto& shot : std::as_const(undrawn)) {
        missing.append(shot.first + QStringLiteral(" - ") + shot.second);
    }
    INFO("shots drawn by no segment: " << joined(missing));
    CHECK(undrawn.isEmpty());
}

} // namespace casePairs
