/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPASSAGETUBECASES_H
#define CWPASSAGETUBECASES_H

//Our includes
#include "CaveWhereLibExport.h"
#include "cwCavingRegionData.h"
#include "cwStationPositionLookup.h"
#include "cwUnits.h"

//Qt includes
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>
#include <QVector3D>

//Std includes
#include <optional>

/**
 * Synthetic surveys for exercising cwPassageTubeBuilder: the hand-made
 * junction cases of the tube gallery, the Phake Cave survey, a parametric
 * sweep over the junction space and seeded random cave trees.
 *
 * Positions are given directly (x east, y north, z up, meters), so a survey
 * needs no shot data or loop closure. Pure functions: callable from any
 * thread.
 */
class CAVEWHERE_LIB_EXPORT cwPassageTubeCases
{
public:
    cwPassageTubeCases() = delete;

    static constexpr double kThroughLegMeters = 10.0;
    static constexpr double kArmLengthMeters = 10.0;

    // A station occurrence in a chunk; lrud holds left, right, up and down
    // as the user would type them, empty for a blank reading.
    struct Station {
        QString name;
        std::optional<QVector3D> position;
        QStringList lrud = {QString(), QString(), QString(), QString()};

        bool operator==(const Station&) const = default;
    };

    // A trip as chunks of stations surveyed in order.
    struct Trip {
        QList<QList<Station>> chunks;
        cwUnits::LengthUnit distanceUnit = cwUnits::Meters;

        bool operator==(const Trip&) const = default;
    };

    // What cwPassageTubeBuilder::build takes.
    struct Survey {
        cwCavingRegionData region;
        QHash<QUuid, cwStationPositionLookup> positionsByCave;
    };

    struct Section {
        double left = 1.0;
        double right = 1.0;
        double up = 1.0;
        double down = 1.0;

        Section scaled(double factor) const;
    };

    // A leg past the station before it: it turns turnDegrees in plan,
    // counter-clockwise, from the leg before and climbs at profileDegrees.
    // at is the section at its far station.
    struct ArmLeg {
        double turnDegrees = 0.0;
        double profileDegrees = 0.0;
        double lengthMeters = kArmLengthMeters;
        Section at;
    };

    // An arm leaves J along planDegrees, measured in plan counter-clockwise from
    // the direction back along the through passage toward t0, and climbs
    // at profileDegrees (90 is a pit dropping into J, −90 a shaft below
    // it). at is the far station's section; atJunction is J's own reading
    // on the arm's chunk when hasJunctionLrud is set; a station without
    // readings, hasLrud unset, takes the builder's default radius. legsBeyond
    // carry the arm on past its far station in a second chunk.
    struct ArmSpec {
        double planDegrees = 90.0;
        double profileDegrees = 0.0;
        double lengthMeters = kArmLengthMeters;
        Section at;
        Section atJunction;
        bool hasJunctionLrud = true;
        bool hasLrud = true;
        QVector<ArmLeg> legsBeyond;
    };

    // The through passage runs t0 → J → t1 heading north into J and climbing
    // at throughProfileDegrees. Past J it turns throughBendDegrees in plan,
    // counter-clockwise (away from an arm at planDegrees 90), and
    // throughBendProfileDegrees in profile.
    struct JunctionCase {
        QString name;
        QStringList tags;
        Section throughSection;
        Section throughAtJunction;
        Section throughPastJunction;
        double throughBendDegrees = 0.0;
        double throughProfileDegrees = 0.0;
        double throughBendProfileDegrees = 0.0;
        QVector<ArmSpec> arms;
        double sampleSpacing = 1.0;
    };

    // One entry of the catalog, ready to build.
    struct Case {
        QString name;
        QStringList tags;
        Trip trip;
        double sampleSpacing = 1.0;
    };

    static Trip toTrip(const JunctionCase& junctionCase);
    static Case toCase(const JunctionCase& junctionCase);

    // One cave with a trip per entry; station positions are shared by name.
    static Survey toSurvey(const QList<Trip>& trips);
    static Survey toSurvey(const Trip& trip);
    static Survey toSurvey(const JunctionCase& junctionCase);

    // Phake Cave, trip "2025.2 Release", with its four chunks as the file
    // has them.
    static Trip phakeCave();

    // The tube gallery's hand-made junctions.
    static QVector<JunctionCase> galleryCases();

    // Every combination of the junction parameters, plus the junction-free
    // variants tagged arms:0. Cases with an arm inside another passage at J
    // are tagged crowded.
    static QVector<JunctionCase> sweepCases();

    // The gallery cases, Phake Cave, the sweep, and the loops and sharp
    // bends tagged arms:0.
    static QVector<Case> catalog();

    // A seeded cave tree with junctionCount three-way junctions, surveyed as
    // a caver would: each passage turns at most kRandomMaxTurnDegrees in plan
    // per leg, a branch leaves its junction at least kRandomMinBranchDegrees
    // from the legs already there, its readings change at most
    // kRandomMaxLrudStep-fold per leg and its legs reach well past its walls,
    // a leg kRandomShaftDegrees or steeper is a shaft whose lower station
    // reads its depth as up and upper station as down, and every station
    // keeps kRandomClearanceMeters from the other passages' legs.
    static constexpr double kRandomMaxTurnDegrees = 60.0;
    static constexpr double kRandomMinBranchDegrees = 30.0;
    static constexpr double kRandomMaxLrudStep = 2.0;
    static constexpr double kRandomShaftDegrees = 45.0;
    static constexpr double kRandomClearanceMeters = 1.0;
    static Trip random(unsigned int seed, int junctionCount);

    // A seeded cave tree with junctionCount three-way junctions, uniform
    // random bearings and independent readings at every station.
    static Trip randomWild(unsigned int seed, int junctionCount);
};

#endif // CWPASSAGETUBECASES_H
