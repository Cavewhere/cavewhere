#pragma once

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"
#include "cwTriangulateStation.h"

//Qt includes
#include <QList>
#include <QPair>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QTransform>
#include <QVector>
#include <QVector3D>

//Std includes
#include <algorithm>
#include <cmath>
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

inline constexpr double kMetersPerFoot = 0.3048;
inline constexpr double kLegMeters = 30.0 * kMetersPerFoot;
// An interpolated station lies on its shot within a millimeter in the world and
// within the same tolerance in note units.
inline constexpr float kOnShotTolerance = 0.001f;

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

//! The file's seven stations solved and keyed the way the reader keys them,
//! in meters with x east and y north: P1 at the origin, P2 and P3 east of it,
//! Q1 south of P3, and the lowercase members 100 ft north of their partners.
inline cwStationPositionLookup solvedLookup()
{
    const auto at = [](double east, double north) {
        return QVector3D(static_cast<float>(east), static_cast<float>(north), 0.0f);
    };
    cwStationPositionLookup lookup;
    lookup.setPositionAtKey(QStringLiteral("P1"), at(0.0, 0.0));
    lookup.setPositionAtKey(QStringLiteral("P2"), at(kLegMeters, 0.0));
    lookup.setPositionAtKey(QStringLiteral("P3"), at(2.0 * kLegMeters, 0.0));
    lookup.setPositionAtKey(QStringLiteral("q1"), at(2.0 * kLegMeters, -kLegMeters));
    lookup.setPositionAtKey(QStringLiteral("p1"), at(0.0, kSeparationMeters));
    lookup.setPositionAtKey(QStringLiteral("p2"), at(kLegMeters, kSeparationMeters));
    lookup.setPositionAtKey(QStringLiteral("p3"), at(2.0 * kLegMeters, kSeparationMeters));
    return lookup;
}

//! The network of solvedLookup(): the file's six shots between solved keys.
inline cwSurveyNetwork solvedNetwork()
{
    cwSurveyNetwork network;
    for (const auto& shot : shots(QString())) {
        network.addShotKeys(shot.first, shot.second);
    }
    const QMap<QString, QVector3D> positions = solvedLookup().positions();
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
        network.setPositionAtKey(it.key(), it.value());
    }
    return network;
}

//! Where a note draws the solved station \a key: the plan, shrunk to fit.
inline QVector3D notePositionOf(const QString& key)
{
    constexpr float kNoteUnitsPerMeter = 0.01f;
    return solvedLookup().position(key) * kNoteUnitsPerMeter;
}

// A page that draws the plan turned counterclockwise, a leg kLegOnNote of the
// page long and P1 at kP1OnNote. North is drawn turned the same way, so the
// note's north up is kTurnedNorthUp.
inline constexpr double kNoteTurnDegrees = 30.0;
inline constexpr double kTurnedNorthUp = 360.0 - kNoteTurnDegrees;
inline constexpr double kLegOnNote = 0.15;
inline const QPointF kP1OnNote(0.3, 0.3);
inline constexpr double kNorthToleranceDegrees = 0.05;

//! The stations the turned page holds, as {typed name, solved key}: P1 - P2 -
//! P3 - Q1, each leg a pair member's own shot, and p2 beside them. Q1 is typed
//! in uppercase and resolves to q1.
inline QList<QPair<QString, QString>> turnedNoteStations()
{
    return {
        {QStringLiteral("P1"), QStringLiteral("P1")},
        {QStringLiteral("P2"), QStringLiteral("P2")},
        {QStringLiteral("P3"), QStringLiteral("P3")},
        {QStringLiteral("Q1"), QStringLiteral("q1")},
        {QStringLiteral("p2"), QStringLiteral("p2")},
    };
}

//! Morph anchors in an order that puts each lowercase member before its
//! partner, so a lookup that folds names lets the uppercase member's note
//! position replace its partner's.
inline QStringList morphAnchors()
{
    return {QStringLiteral("p1"), QStringLiteral("p2"), QStringLiteral("P1"), QStringLiteral("P2")};
}

//! The shots between morphAnchors(), as solved keys.
inline QList<QPair<QString, QString>> morphAnchorShots()
{
    return {{QStringLiteral("P1"), QStringLiteral("P2")},
            {QStringLiteral("P1"), QStringLiteral("p1")},
            {QStringLiteral("p1"), QStringLiteral("p2")}};
}

//! Where the turned page draws the solved station \a key, in normalized note
//! coordinates with y up.
inline QPointF turnedOnNote(const QString& key)
{
    const QVector3D world = solvedLookup().position(key);
    QTransform toNote;
    toNote.translate(kP1OnNote.x(), kP1OnNote.y());
    toNote.rotate(kNoteTurnDegrees);
    toNote.scale(kLegOnNote / kLegMeters, kLegOnNote / kLegMeters);
    return toNote.map(world.toPointF());
}

//! Checks the stations a carpet morphs with when the note holds \a anchors.
//! Each anchor sits at its own solved position, and each shot of \a noteShots
//! (solved keys, both ends on the note) carries ceil(length / spacing) - 1
//! interpolated stations, each on that shot at the same fraction in the world
//! and on the note.
inline void checkInterpolatedShots(const QList<cwTriangulateStation>& stations,
                                   const QStringList& anchors,
                                   const QList<QPair<QString, QString>>& noteShots,
                                   double spacingMeters)
{
    const cwStationPositionLookup lookup = solvedLookup();

    QList<cwTriangulateStation> interpolated;
    QStringList placedAnchors;
    for (const cwTriangulateStation& station : stations) {
        if (anchors.contains(station.name())) {
            INFO("anchor " << station.name().toStdString());
            REQUIRE(samePoint(station.position(), lookup.position(station.name())));
            placedAnchors.append(station.name());
        } else {
            interpolated.append(station);
        }
    }
    REQUIRE(joined(placedAnchors) == joined(anchors));

    QList<int> counts(noteShots.size(), 0);
    QStringList offShot;
    for (const cwTriangulateStation& station : std::as_const(interpolated)) {
        const auto shotIndex = [&]() -> qsizetype {
            for (qsizetype i = 0; i < noteShots.size(); ++i) {
                const QVector3D fromWorld = lookup.position(noteShots.at(i).first);
                const QVector3D toWorld = lookup.position(noteShots.at(i).second);
                const QVector3D fromNote = notePositionOf(noteShots.at(i).first);
                const QVector3D toNote = notePositionOf(noteShots.at(i).second);
                const QVector3D world = toWorld - fromWorld;
                const float t = QVector3D::dotProduct(station.position() - fromWorld, world)
                                / world.lengthSquared();
                if (t <= 0.0f || t >= 1.0f) {
                    continue;
                }
                const bool onWorldShot = station.position().distanceToPoint(fromWorld + world * t)
                                         < kOnShotTolerance;
                const bool onNoteShot = station.notePosition().distanceToPoint(
                                            fromNote + (toNote - fromNote) * t)
                                        < kOnShotTolerance;
                if (onWorldShot && onNoteShot) {
                    return i;
                }
            }
            return -1;
        }();
        if (shotIndex >= 0) {
            counts[shotIndex]++;
        } else {
            offShot.append(station.name());
        }
    }

    INFO("interpolated stations on no note shot, in the world and on the note: " << joined(offShot));
    CHECK(offShot.isEmpty());
    for (qsizetype i = 0; i < noteShots.size(); ++i) {
        const QPair<QString, QString>& shot = noteShots.at(i);
        const double length = static_cast<double>(
            (lookup.position(shot.second) - lookup.position(shot.first)).length());
        const int expected = static_cast<int>(std::ceil(length / spacingMeters)) - 1;
        INFO("shot " << shot.first.toStdString() << " - " << shot.second.toStdString());
        CHECK(counts.at(i) == expected);
    }
}

} // namespace casePairs
