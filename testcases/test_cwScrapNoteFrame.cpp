/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "LoadProjectHelper.h"

//Cavewhere includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwLinePlotManager.h"
#include "cwNote.h"
#include "cwNoteStation.h"
#include "cwNoteTranformation.h"
#include "cwProject.h"
#include "cwScrap.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "GeoreferenceFixtureHelper.h"

//Qt includes
#include <QLineF>
#include <QMatrix4x4>

//Std includes
#include <algorithm>

namespace {
    constexpr double kDeclination = 12.5;

    // A shot leader only has to land close enough to the drawn station for the
    // surveyor to see which one it points at. The note transform is a
    // least-squares fit over every station, so a couple of tenths of a percent
    // of the note's width is the noise floor, not a defect.
    constexpr double kMaxLeaderError = 0.05;

    cwScrap* firstScrap(const cwProject* project) {
        return project->cavingRegion()->cave(0)->trip(0)->notes()->notes().at(0)->scrap(0);
    }

    // The world -> normalized-note composition cwScrapStationView::updateShotLines()
    // builds for plan and projected scraps, evaluated at a neighbor's plotted
    // position. That point is where the leader line's far end is drawn.
    QPointF leaderEndpoint(cwScrap* scrap,
                           const cwNoteStation& from,
                           const QVector3D& fromWorld,
                           const QVector3D& neighborWorld)
    {
        cwNote* note = scrap->parentNote();

        QMatrix4x4 notePageAspect = note->scaleMatrix().inverted();

        QMatrix4x4 offsetMatrix;
        offsetMatrix.translate(-fromWorld);

        QMatrix4x4 dotPerMeter;
        dotPerMeter.scale(note->dotPerMeter(), note->dotPerMeter(), 1.0);

        QMatrix4x4 noteStationOffset;
        noteStationOffset.translate(QVector3D(from.positionOnNote()));

        const QMatrix4x4 toNormalizedNote = noteStationOffset
                * dotPerMeter
                * notePageAspect
                * scrap->resolvedPlacement().worldToPageMatrix()
                * offsetMatrix;

        return toNormalizedNote.map(neighborWorld).toPointF();
    }

    // Every shot the scrap actually draws: a station on the note, paired with a
    // neighbor that is also drawn on the note. Both checks below walk exactly
    // this set, so they can never end up covering different shots.
    template <typename Visitor>
    void forEachDrawnShot(cwScrap* scrap, cwTrip* trip, Visitor&& visit) {
        const QList<cwNoteStation> stations = scrap->stations();

        for(const cwNoteStation& noteStation : stations) {
            const QSet<cwStation> neighbors = trip->neighboringStations(noteStation.name());
            for(const cwStation& neighbor : neighbors) {
                auto drawn = std::find_if(stations.begin(), stations.end(),
                                          [&neighbor](const cwNoteStation& station) {
                    return station.name().compare(neighbor.name(), Qt::CaseInsensitive) == 0;
                });
                if(drawn == stations.end()) { continue; }

                visit(noteStation, *drawn);
            }
        }
    }

    // Worst distance, over every drawn shot in the scrap, between a leader line's
    // far end and the station it points at - both in normalized note coordinates.
    double worstLeaderError(cwProject* project) {
        cwScrap* scrap = firstScrap(project);
        cwCave* cave = project->cavingRegion()->cave(0);
        const cwStationPositionLookup lookup = cave->stationPositionLookup();

        double worst = 0.0;
        forEachDrawnShot(scrap, cave->trip(0),
                         [&](const cwNoteStation& from, const cwNoteStation& drawn) {
            if(!lookup.hasPosition(from.name())) { return; }
            if(!lookup.hasPosition(drawn.name())) { return; }

            const QPointF endpoint = leaderEndpoint(scrap,
                                                    from,
                                                    lookup.position(from.name()),
                                                    lookup.position(drawn.name()));
            worst = std::max(worst, QLineF(endpoint, drawn.positionOnNote()).length());
        });
        return worst;
    }

    // Every neighbor of every drawn station, asked for by its note position -
    // exactly what a surveyor gets when clicking along a shot leader.
    bool guessesEveryNeighborName(cwProject* project) {
        cwScrap* scrap = firstScrap(project);
        bool everyGuessCorrect = true;

        forEachDrawnShot(scrap, project->cavingRegion()->cave(0)->trip(0),
                         [&](const cwNoteStation& from, const cwNoteStation& drawn) {
            const QString guess = scrap->guessNeighborStationName(from, drawn.positionOnNote());
            if(guess.compare(drawn.name(), Qt::CaseInsensitive) != 0) {
                UNSCOPED_INFO("From " << from.name().toStdString()
                              << " expected " << drawn.name().toStdString()
                              << " but guessed " << guess.toStdString());
                everyGuessCorrect = false;
            }
        });
        return everyGuessCorrect;
    }

    // A loaded project with its line plot solved, and the scrap's fitted note
    // transform frozen so the stored north behaves like a hand-drawn north arrow
    // - nothing re-fits it away when the plot rotates underneath.
    struct PlottedProject {
        std::shared_ptr<cwProject> project;
        std::unique_ptr<cwLinePlotManager> plotManager;

        cwCave* cave() const { return project->cavingRegion()->cave(0); }
        cwTrip* trip() const { return cave()->trip(0); }
        cwScrap* scrap() const { return firstScrap(project.get()); }
        void replot() { plotManager->waitToFinish(); }
    };

    PlottedProject loadPlottedProject(const QString& dataset) {
        PlottedProject plotted;
        plotted.project = fileToProject(testcasesDatasetPath(dataset));
        plotted.plotManager = std::make_unique<cwLinePlotManager>();

        //Mirrors what cwScrapManager does when the plot moves stations
        QObject::connect(plotted.plotManager.get(),
                         &cwLinePlotManager::stationPositionInScrapsChanged,
                         [](const QList<cwScrap*>& scraps) {
                             for(cwScrap* scrap : scraps) {
                                 scrap->updateNoteTransformation();
                             }
                         });
        plotted.plotManager->setRegion(plotted.project->cavingRegion());
        plotted.replot();

        plotted.scrap()->setCalculateNoteTransform(false);
        return plotted;
    }

    // Rotates the plot out from under the frozen note: first a manual
    // declination, then a coordinate system that adds grid convergence.
    void addDeclination(PlottedProject& plotted) {
        plotted.trip()->calibrations()->setAutoDeclination(false);
        plotted.trip()->calibrations()->setDeclinationManual(kDeclination);
        plotted.replot();
    }

    void addCoordinateSystem(PlottedProject& plotted) {
        const double convergence = cwGeoreferenceFixture::georeferenceEastOfUtm13N(
                    plotted.cave(), plotted.scrap()->stations().first().name());
        REQUIRE(convergence > 0.1);
        plotted.replot();
    }
}

TEST_CASE("Shot leader lines point at the station they name", "[cwScrapNoteFrame]") {
    // A shot leader maps a neighbor's grid-aligned plot position onto the note.
    // The plot rotates when declination or a coordinate system is added, but the
    // note doesn't - the scrap's stored north is what the surveyor drew. The
    // resolution in resolvedPlacement() is what cancels that rotation, so the
    // leaders must keep pointing at the same drawn stations throughout
    // (issues #628, #644).

    const QString dataset = GENERATE(QStringLiteral("scrapGuessNeighbor/scrapGuessNeigborPlan.cw"),
                                     QStringLiteral("scrapAutoCalculate/ProjectProfile-test-v3.cw"));
    INFO("Dataset: " << dataset.toStdString());

    PlottedProject plotted = loadPlottedProject(dataset);
    const double storedNorth = plotted.scrap()->noteTransformation()->northUp();

    CHECK(worstLeaderError(plotted.project.get()) < kMaxLeaderError);

    addDeclination(plotted);
    CHECK(worstLeaderError(plotted.project.get()) < kMaxLeaderError);

    addCoordinateSystem(plotted);
    CHECK(worstLeaderError(plotted.project.get()) < kMaxLeaderError);

    // The note itself never moved - only the resolution absorbed the rotation.
    CHECK(plotted.scrap()->noteTransformation()->northUp() == storedNorth);
}

TEST_CASE("Station names are guessed in the scrap's local frame", "[cwScrapNoteFrame]") {
    // guessNeighborStationName() picks the neighbor whose plotted position lands
    // closest to a note position, so it rides on the same world -> note bridge as
    // the shot leaders. Grid convergence was missing from that bridge, which put
    // a georeferenced plan note's guesses on the wrong station once the survey
    // was far enough from the central meridian.

    PlottedProject plotted =
            loadPlottedProject(QStringLiteral("scrapGuessNeighbor/scrapGuessNeigborPlan.cw"));

    CHECK(guessesEveryNeighborName(plotted.project.get()));

    addDeclination(plotted);
    CHECK(guessesEveryNeighborName(plotted.project.get()));

    addCoordinateSystem(plotted);
    CHECK(guessesEveryNeighborName(plotted.project.get()));
}
