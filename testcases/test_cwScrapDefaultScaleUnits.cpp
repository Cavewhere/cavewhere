/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwImage.h"
#include "cwImageResolution.h"
#include "cwLength.h"
#include "cwNote.h"
#include "cwNoteStation.h"
#include "cwNoteTranformation.h"
#include "cwFutureManagerModel.h"
#include "cwPlanScrapViewMatrix.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwScale.h"
#include "cwScrap.h"
#include "cwScrapData.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwUnits.h"

//Test includes
#include "LoadProjectHelper.h"
#include "TestHelper.h"
#include "cwSignalSpy.h"

//Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QSize>
#include <QTemporaryDir>
#include <QVector3D>

//Std includes
#include <memory>

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace Catch;

namespace {

    struct AutoScaledScrap {
        cwTrip* trip = nullptr;
        cwScrap* scrap = nullptr;
    };

    //! A real 1000x1000 image on disk, so a fixture that goes through a save can
    //! carry a note the image pipeline is able to open and crop.
    QString writeNoteImage(const QString& directory)
    {
        const QString path = QDir(directory).filePath(QStringLiteral("note.png"));
        QImage image(1000, 1000, QImage::Format_RGB32);
        image.fill(Qt::white);
        REQUIRE(image.save(path));
        return path;
    }

    //! Adds a 1:100 plan scrap to \a region: a2 sits 20 m north of a1 in the
    //! plot, drawn as a 0.2 m line on a 1 m x 1 m page. That's the scale from
    //! issue #646, where the auto transform read "0.03 m = 2.54 m" — one inch,
    //! converted. \a surveyUnit is the trip's distance unit, which is what the
    //! scale should read in; the region is put on the other system so a check
    //! can't confuse the two.
    AutoScaledScrap addAutoScaledScrapAt1To100(cwCavingRegion* region,
                                               cwUnits::LengthUnit surveyUnit,
                                               const QString& imagePath = QStringLiteral("note.png"))
    {
        region->setUnitSystem(cwUnits::unitSystem(surveyUnit) == cwUnits::Imperial
                                  ? cwUnits::Metric
                                  : cwUnits::Imperial);

        auto* cave = new cwCave();
        cave->setName(QStringLiteral("Cave"));
        region->addCave(cave);

        auto* trip = new cwTrip();
        trip->setName(QStringLiteral("Trip"));
        cave->addTrip(trip);
        trip->calibrations()->setDistanceUnit(surveyUnit);
        REQUIRE(trip->unitSystem() != cwUnits::unitSystem(surveyUnit));

        //Give the note a valid path-mode image before it enters the tree — under
        //a real cwProject (the cwScrapManager case below) the save path asserts
        //on the mode as soon as the note is reachable
        cwImage image;
        image.setOriginalSize(QSize(1000, 1000));
        image.setPath(imagePath);

        auto* note = new cwNote();
        note->setImage(image);
        note->imageResolution()->setUnit(cwUnits::DotsPerMeter);
        note->imageResolution()->setValue(1000.0);
        trip->notes()->addNotes({ note });

        auto* scrap = new cwScrap();
        note->addScrap(scrap);
        scrap->setType(cwScrap::Plan);

        cwStationPositionLookup lookup;
        lookup.setPosition(QStringLiteral("a1"), QVector3D(0.0f, 0.0f, 0.0f));
        lookup.setPosition(QStringLiteral("a2"), QVector3D(0.0f, 20.0f, 0.0f));
        cave->setStationPositionLookup(lookup);

        cwSurveyNetwork network;
        network.addShot(QStringLiteral("a1"), QStringLiteral("a2"));
        cave->setSurveyNetwork(network);

        cwNoteStation station1;
        station1.setName(QStringLiteral("a1"));
        station1.setPositionOnNote(QPointF(0.5, 0.4));
        cwNoteStation station2;
        station2.setName(QStringLiteral("a2"));
        station2.setPositionOnNote(QPointF(0.5, 0.6));
        scrap->setStations({ station1, station2 });

        REQUIRE(scrap->calculateNoteTransform());

        return { trip, scrap };
    }

    //! What that 1:100 scrap should read for a trip surveyed in \a surveyUnit:
    //! the survey unit in cave, its paper companion on paper. Only the imperial
    //! denominator is a number other than 1, so a check that skips imperial
    //! can't tell "resolved from the trip" from "hardcoded metric".
    void checkReadsAt1To100(cwNoteTranformation* transform, cwUnits::LengthUnit surveyUnit)
    {
        const bool imperial = cwUnits::unitSystem(surveyUnit) == cwUnits::Imperial;
        INFO("Survey unit: " << cwUnits::unitName(surveyUnit).toStdString());

        CHECK(transform->scaleNumerator()->unit() == (imperial ? cwUnits::Inches : cwUnits::Centimeters));
        CHECK(transform->scaleNumerator()->value() == Approx(1.0));
        CHECK(transform->scaleDenominator()->unit() == surveyUnit);
        CHECK(transform->scaleDenominator()->value() == Approx(imperial ? 100.0 / 12.0 : 1.0));
        CHECK(transform->scale() == Approx(0.01));
    }
}

// The shared paper-scale default both the sketch map scale and a new scrap's
// note-transformation scale round to.
TEST_CASE("cwScale::defaultData gives the project unit system's paper scale",
          "[ScrapDefaultScale]")
{
    SECTION("metric is 1 cm = 2.5 m") {
        const cwScale::Data d = cwScale::defaultData(cwUnits::Metric);
        CHECK(d.scaleNumerator.unit == cwUnits::Centimeters);
        CHECK(d.scaleNumerator.value == Approx(1.0));
        CHECK(d.scaleDenominator.unit == cwUnits::Meters);
        CHECK(d.scaleDenominator.value == Approx(2.5));
    }

    SECTION("imperial is 1 in = 20 ft") {
        const cwScale::Data d = cwScale::defaultData(cwUnits::Imperial);
        CHECK(d.scaleNumerator.unit == cwUnits::Inches);
        CHECK(d.scaleNumerator.value == Approx(1.0));
        CHECK(d.scaleDenominator.unit == cwUnits::Feet);
        CHECK(d.scaleDenominator.value == Approx(20.0));
    }
}

// Until a scrap has stations there's nothing to derive a scale from, so the seed
// is what the editor shows. It should follow the project unit system rather than
// cwScale's raw-inches default.
TEST_CASE("cwScrap::seedDefaultScale sets the note-transformation display units",
          "[ScrapDefaultScale]")
{
    cwScrap scrap;

    SECTION("metric seeds cm / m") {
        scrap.seedDefaultScale(cwUnits::Metric);
        CHECK(scrap.noteTransformation()->scaleNumerator()->unit() == cwUnits::Centimeters);
        CHECK(scrap.noteTransformation()->scaleDenominator()->unit() == cwUnits::Meters);
    }

    SECTION("imperial seeds in / ft") {
        scrap.seedDefaultScale(cwUnits::Imperial);
        CHECK(scrap.noteTransformation()->scaleNumerator()->unit() == cwUnits::Inches);
        CHECK(scrap.noteTransformation()->scaleDenominator()->unit() == cwUnits::Feet);
    }
}

// Issue #646: the calculator emits the scale as 1 in : N in, so unit-converting
// both halves left the on-paper side reading one inch in the project's units.
// The recompute pins the on-paper side to 1 and puts the ratio in the cave side.
TEST_CASE("An auto-calculated scale reads as a round paper scale",
          "[ScrapDefaultScale]")
{
    const cwUnits::LengthUnit surveyUnit = GENERATE(cwUnits::Meters, cwUnits::Feet);

    auto region = std::make_unique<cwCavingRegion>();
    auto plan = addAutoScaledScrapAt1To100(region.get(), surveyUnit);
    plan.scrap->updateNoteTransformation();

    checkReadsAt1To100(plan.scrap->noteTransformation(), surveyUnit);
}

// An auto-calculated scale isn't saved, so a loaded scrap has no stored display
// units — it used to inherit cwNoteTransformationData's 1 m / 1 m default and
// read "0.03 m = 2.54 m". The units come from the trip instead, no matter what
// the scrap happens to be holding.
TEST_CASE("An auto-calculated scale takes its units from the trip",
          "[ScrapDefaultScale]")
{
    const cwUnits::LengthUnit surveyUnit = GENERATE(cwUnits::Meters, cwUnits::Feet);

    auto region = std::make_unique<cwCavingRegion>();
    auto plan = addAutoScaledScrapAt1To100(region.get(), surveyUnit);
    cwNoteTranformation* transform = plan.scrap->noteTransformation();

    SECTION("a recompute overrides the units the scrap is holding") {
        transform->scaleNumerator()->setUnit(cwUnits::Meters);
        transform->scaleDenominator()->setUnit(cwUnits::Meters);

        plan.scrap->updateNoteTransformation();

        checkReadsAt1To100(transform, surveyUnit);
    }

    SECTION("setData re-derives the scale rather than adopting the stored one") {
        cwScrapData data;
        data.id = plan.scrap->id();
        data.outlinePoints = plan.scrap->points();
        data.stations = plan.scrap->stations();
        data.calculateNoteTransform = true;
        data.viewMatrix = std::make_unique<cwPlanScrapViewMatrix::Data>();

        SECTION("with no stored scale, as the proto loader hands it over") {
            REQUIRE(data.noteTransformation.scale.scaleNumerator.unit == cwUnits::Meters);
        }

        SECTION("with a stored scale, as cwScrapMergeApplier hands it over") {
            data.noteTransformation.scale.scaleNumerator = { cwUnits::Yards, 42.0, false };
            data.noteTransformation.scale.scaleDenominator = { cwUnits::Miles, 7.0, false };
        }

        plan.scrap->setData(data);

        checkReadsAt1To100(transform, surveyUnit);
    }
}

// Derived units go stale whenever what they're derived from moves: the trip
// switches survey units, or a load reads them before the scrap's parents are
// attached (and gets distanceUnit()'s fallback). cwScrapManager calls this on
// both — on cwTripCalibration::distanceUnitChanged, and as each scrap enters
// the region tree.
TEST_CASE("An auto-calculated scale re-resolves its units against the trip",
          "[ScrapDefaultScale]")
{
    auto region = std::make_unique<cwCavingRegion>();
    auto plan = addAutoScaledScrapAt1To100(region.get(), cwUnits::Meters);
    cwNoteTranformation* transform = plan.scrap->noteTransformation();

    SECTION("switching the trip's survey unit relabels the same ratio") {
        checkReadsAt1To100(transform, cwUnits::Meters);

        plan.trip->calibrations()->setDistanceUnit(cwUnits::Feet);
        plan.scrap->updateNoteTransformUnits();

        checkReadsAt1To100(transform, cwUnits::Feet);
    }

    SECTION("a manual scale keeps the units the user picked") {
        plan.scrap->setCalculateNoteTransform(false);
        transform->scaleNumerator()->setUnit(cwUnits::Meters);
        transform->scaleDenominator()->setUnit(cwUnits::Meters);

        plan.trip->calibrations()->setDistanceUnit(cwUnits::Feet);
        plan.scrap->updateNoteTransformUnits();

        CHECK(transform->scaleNumerator()->unit() == cwUnits::Meters);
        CHECK(transform->scaleDenominator()->unit() == cwUnits::Meters);
    }
}

// The test above calls the relabel by hand; nothing in the app does. This runs
// the same switch through the wiring a user hits — a scrap living in a real
// cwRootData, where cwScrapManager is what connects the trip's calibration to
// the scrap. Toggling the trip between m and ft has to move the scale in both
// directions on its own.
TEST_CASE("Switching a trip's distance unit relabels its auto scales",
          "[ScrapDefaultScale]")
{
    auto rootData = std::make_unique<cwRootData>();
    auto plan = addAutoScaledScrapAt1To100(rootData->region(), cwUnits::Meters);
    cwNoteTranformation* transform = plan.scrap->noteTransformation();

    checkReadsAt1To100(transform, cwUnits::Meters);

    plan.trip->calibrations()->setDistanceUnit(cwUnits::Feet);
    checkReadsAt1To100(transform, cwUnits::Feet);

    plan.trip->calibrations()->setDistanceUnit(cwUnits::Meters);
    checkReadsAt1To100(transform, cwUnits::Meters);
}

// Issue #646's actual symptom. An auto-calculated scale isn't persisted, so a
// loaded scrap comes back holding cwNoteTransformationData's 1 m / 1 m default
// and reads it before cwScrapManager attaches it to a trip. Only the relabel in
// connectScrap gets it onto the trip's units — a live scrap never has them wrong,
// so nothing above this covers the load.
TEST_CASE("A loaded auto scale reads in the trip's units",
          "[ScrapDefaultScale]")
{
    QTemporaryDir tempDir(QDir::tempPath()
                          + QStringLiteral("/test_cwScrapDefaultScaleUnits-%1-XXXXXX")
                                .arg(QCoreApplication::applicationPid()));
    REQUIRE(tempDir.isValid());

    QString savedProjectPath;

    {
        auto root = std::make_unique<cwRootData>();
        root->account()->setName(QStringLiteral("Scale Units Test"));
        root->account()->setEmail(QStringLiteral("scale.units.test@example.com"));

        auto plan = addAutoScaledScrapAt1To100(root->project()->cavingRegion(),
                                               cwUnits::Feet,
                                               writeNoteImage(tempDir.path()));
        plan.scrap->updateNoteTransformation();
        checkReadsAt1To100(plan.scrap->noteTransformation(), cwUnits::Feet);

        REQUIRE(root->project()->saveAs(QDir(tempDir.path())
                                            .filePath(QStringLiteral("auto-scale.cwproj"))));
        root->futureManagerModel()->waitForFinished();
        root->project()->waitSaveToFinish();
        savedProjectPath = root->project()->filename();
        REQUIRE(!savedProjectPath.isEmpty());
    }

    auto reopened = std::make_unique<cwRootData>();
    addTokenManager(reopened->project());
    reopened->project()->newProject();
    reopened->project()->loadOrConvert(savedProjectPath);
    reopened->project()->waitLoadToFinish();
    reopened->futureManagerModel()->waitForFinished();

    cwCavingRegion* region = reopened->project()->cavingRegion();
    REQUIRE(region->caveCount() == 1);
    cwTrip* trip = region->cave(0)->trip(0);
    REQUIRE(trip->calibrations()->distanceUnit() == cwUnits::Feet);
    REQUIRE(trip->notes()->notes().size() == 1);
    REQUIRE(trip->notes()->notes().at(0)->scraps().size() == 1);

    cwScrap* scrap = trip->notes()->notes().at(0)->scraps().at(0);
    REQUIRE(scrap->calculateNoteTransform());

    //The relabel rides the scrap's arrival in the region tree, which lands on the
    //event loop after waitLoadToFinish() returns — poll rather than assume it has
    waitUntil([scrap]() {
        return scrap->noteTransformation()->scaleNumerator()->unit() == cwUnits::Inches;
    });

    CHECK(scrap->noteTransformation()->scaleNumerator()->unit() == cwUnits::Inches);
    CHECK(scrap->noteTransformation()->scaleDenominator()->unit() == cwUnits::Feet);
}

// The relabel moves the units without moving the ratio, so it must not look like
// a geometry change — cwScrapManager morphs a scrap on scaleChanged, and a whole
// trip's worth of scraps re-cropping for an identical result is the cost of
// getting this wrong. cwScale draws the line: scaleChanged is the ratio,
// scaleDataChanged is the stored data that persistence follows.
TEST_CASE("Relabeling an auto scale moves its units, not its scale",
          "[ScrapDefaultScale]")
{
    auto region = std::make_unique<cwCavingRegion>();
    auto plan = addAutoScaledScrapAt1To100(region.get(), cwUnits::Meters);
    cwNoteTranformation* transform = plan.scrap->noteTransformation();
    plan.scrap->updateNoteTransformation();

    cwSignalSpy scaleSpy(transform, &cwNoteTranformation::scaleChanged);
    cwSignalSpy dataSpy(transform, &cwNoteTranformation::scaleDataChanged);

    SECTION("switching the trip's unit relabels without a geometry change") {
        plan.trip->calibrations()->setDistanceUnit(cwUnits::Feet);
        plan.scrap->updateNoteTransformUnits();

        checkReadsAt1To100(transform, cwUnits::Feet);
        CHECK(scaleSpy.count() == 0);
        CHECK(dataSpy.count() > 0);
    }

    SECTION("a relabel to the units it already has is silent") {
        plan.scrap->updateNoteTransformUnits();

        CHECK(scaleSpy.count() == 0);
        CHECK(dataSpy.count() == 0);
    }

    SECTION("an actual ratio change still reports one") {
        transform->scaleDenominator()->setValue(2.0);

        CHECK(scaleSpy.count() == 1);
        CHECK(dataSpy.count() == 1);
    }
}
