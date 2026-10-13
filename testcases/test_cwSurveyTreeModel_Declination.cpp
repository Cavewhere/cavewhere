/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwDeclination.h"
#include "cwGeoPoint.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSurveyTreeModel.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "ExternalCenterlineTestHelpers.h"
#include "TestHelper.h"

// Qt
#include <QMetaEnum>
#include <QSignalSpy>

namespace {

const QString kDeclinationFixture = QStringLiteral("survex_declination/declination.svx");
const QString kSilentFixture = QStringLiteral("survex_declination/silent.svx");
const QString kMetadataFixture = QStringLiteral("survex_with_metadata.svx");
const QString kAutoCoordinateSystem = QStringLiteral("EPSG:32616");
const cwGeoPoint kAutoLocation(600000.0, 4100000.0, 200.0);
const QDate kFixtureDate(2020, 6, 1);
const QDate kRedatedDate(1990, 1, 1);

constexpr double kFileSetDegrees = -2.5; // *calibrate declination 2.5
constexpr double kModernDegrees = 3.25;  // *declination 3.25 degrees
constexpr double kChildDegrees = -1.0;   // parent's *calibrate declination 1
constexpr double kMetadataDegrees = -7.2; // *calibrate declination 7.2
constexpr double kManualDegrees = 4.5;
constexpr int kDegreesPlaces = 4;

std::string expectedDeclination(const char* origin, double degrees)
{
    return QStringLiteral("%1 %2").arg(QLatin1String(origin)).arg(degrees, 0, 'f', kDegreesPlaces).toStdString();
}

cwTrip* tripNamed(const cwSurveyNode* node, const QString& name)
{
    for (cwTrip* trip : node->trips()) {
        if (trip->name() == name) {
            return trip;
        }
    }
    return nullptr;
}

//! A trip row's Decl cell as "<origin> <degrees>": the DeclinationOrigin's key,
//! then DeclinationRole to four places. "missing" stands for an origin the
//! model does not give.
std::string rowDeclination(const cwSurveyTreeModel& model, cwTrip* trip)
{
    const QModelIndex index = model.indexOf(trip);
    REQUIRE(index.isValid());
    const QVariant origin = model.data(index, cwSurveyTreeModel::DeclinationOriginRole);
    const double degrees = model.data(index, cwSurveyTreeModel::DeclinationRole).toDouble();
    const char* originKey = origin.isValid()
            ? QMetaEnum::fromType<cwSurveyTreeModel::DeclinationOrigin>().valueToKey(origin.toInt())
            : nullptr;
    return expectedDeclination(originKey != nullptr ? originKey : "missing", degrees);
}

//! The origin a native trip's row reads from its own calibration.
const char* calibrationOrigin(const cwTrip* trip)
{
    const cwTripCalibration* calibration = trip->calibrations();
    return calibration->autoDeclination() && calibration->autoDeclinationAvailable()
            ? "DeclinationAuto"
            : "DeclinationManual";
}

double igrfAt(const QDateTime& date)
{
    const auto declination = cwDeclination::compute(kAutoLocation, kAutoCoordinateSystem, date);
    REQUIRE_FALSE(declination.hasError());
    return declination.value();
}

//! True when \a spy saw a dataChanged covering \a rowIndex's Decl cell for DeclinationRole.
bool sawDeclinationChange(const QSignalSpy& spy, const QModelIndex& rowIndex)
{
    for (const QList<QVariant>& arguments : spy) {
        const QModelIndex topLeft = arguments.at(0).value<QModelIndex>();
        const QModelIndex bottomRight = arguments.at(1).value<QModelIndex>();
        const QList<int> roles = arguments.at(2).value<QList<int>>();
        if (topLeft.parent() == rowIndex.parent()
            && topLeft.row() <= rowIndex.row()
            && bottomRight.row() >= rowIndex.row()
            && topLeft.column() <= cwSurveyTreeModel::Decl
            && bottomRight.column() >= cwSurveyTreeModel::Decl
            && (roles.isEmpty() || roles.contains(cwSurveyTreeModel::DeclinationRole))) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("Attached trip rows show the declination cavern applies from the file",
          "[SurveyTreeModel][Attach][Declination]")
{
    auto fixture = makeSavedProject(QStringLiteral("tree-declination"),
                                    QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    cwCavingRegion* region = fixture->project->cavingRegion();

    cwSurveyTreeModel model;
    model.setRegion(region);

    cwSurveyNode* root = attachSourceUnderRegion(fixture.get(), fixturePath(kDeclinationFixture));
    drainPipelines(fixture.get());

    cwTrip* fileset = tripNamed(root, QStringLiteral("fileset"));
    cwTrip* modern = tripNamed(root, QStringLiteral("modern"));
    cwTrip* autoset = tripNamed(root, QStringLiteral("autoset"));
    cwTrip* silent = tripNamed(root, QStringLiteral("silent"));
    cwTrip* mixed = tripNamed(root, QStringLiteral("mixed"));
    cwSurveyNode* parent = childNodeNamed(root, QStringLiteral("parent"));
    REQUIRE(parent != nullptr);
    cwTrip* child = tripNamed(parent, QStringLiteral("child"));
    REQUIRE(fileset != nullptr);
    REQUIRE(modern != nullptr);
    REQUIRE(autoset != nullptr);
    REQUIRE(silent != nullptr);
    REQUIRE(mixed != nullptr);
    REQUIRE(child != nullptr);
    REQUIRE(autoset->date().date() == kFixtureDate);

    SECTION("A block's own value reads as a file value, signed as cavern applies it")
    {
        CHECK(rowDeclination(model, fileset) == expectedDeclination("DeclinationFile", kFileSetDegrees));
        CHECK(rowDeclination(model, modern) == expectedDeclination("DeclinationFile", kModernDegrees));
        CHECK(rowDeclination(model, child) == expectedDeclination("DeclinationFile", kChildDegrees));
    }

    SECTION("A block's *declination auto reads as IGRF at its point on the trip's date")
    {
        CHECK(rowDeclination(model, autoset)
              == expectedDeclination("DeclinationFileAuto", igrfAt(autoset->date())));
    }

    SECTION("A block that sets nothing reads as cavern's 0")
    {
        CHECK(rowDeclination(model, silent) == expectedDeclination("DeclinationNone", 0.0));
    }

    SECTION("A block whose shots take two declinations has no one value")
    {
        CHECK(rowDeclination(model, mixed) == expectedDeclination("DeclinationUnknown", 0.0));
    }

    SECTION("Redating an auto trip announces its new IGRF value")
    {
        const QDateTime redated(kRedatedDate, QTime());
        REQUIRE(expectedDeclination("", igrfAt(redated)) != expectedDeclination("", igrfAt(autoset->date())));
        QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);

        autoset->setDate(redated);

        CHECK(sawDeclinationChange(dataChanged, model.indexOf(autoset)));
        CHECK(rowDeclination(model, autoset) == expectedDeclination("DeclinationFileAuto", igrfAt(redated)));
    }
}

TEST_CASE("A trip-level attachment whose file is silent reads as a native trip",
          "[SurveyTreeModel][Attach][Declination]")
{
    auto fixture = makeSavedProject(QStringLiteral("tree-declination-silent"),
                                    QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    cwSurveyTreeModel model;
    model.setRegion(fixture->project->cavingRegion());

    cwTrip* trip = fixture->trip;
    attachThroughManager(fixture.get(), trip, fixturePath(kSilentFixture));
    drainPipelines(fixture.get());
    REQUIRE(trip->externallyBacked());

    // The driver injects the trip's own calibration, so the row shows it.
    CHECK(rowDeclination(model, trip)
          == expectedDeclination(calibrationOrigin(trip), trip->calibrations()->declination()));

    trip->calibrations()->setAutoDeclination(false);
    trip->calibrations()->setDeclinationManual(kManualDegrees);

    CHECK(rowDeclination(model, trip) == expectedDeclination("DeclinationManual", kManualDegrees));
}

TEST_CASE("A trip-level attachment whose file sets a value reads as that file value",
          "[SurveyTreeModel][Attach][Declination]")
{
    auto fixture = makeSavedProject(QStringLiteral("tree-declination-metadata"),
                                    QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    cwSurveyTreeModel model;
    model.setRegion(fixture->project->cavingRegion());

    cwTrip* trip = fixture->trip;
    attachThroughManager(fixture.get(), trip, fixturePath(kMetadataFixture));
    drainPipelines(fixture.get());

    CHECK(rowDeclination(model, trip) == expectedDeclination("DeclinationFile", kMetadataDegrees));
}

TEST_CASE("A native trip row names its calibration as the declination's origin",
          "[SurveyTreeModel][Declination]")
{
    auto fixture = makeNewProject(QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyTreeModel model;
    model.setRegion(fixture->project->cavingRegion());

    cwTrip* trip = fixture->trip;
    CHECK(rowDeclination(model, trip)
          == expectedDeclination(calibrationOrigin(trip), trip->calibrations()->declination()));

    trip->calibrations()->setAutoDeclination(false);
    trip->calibrations()->setDeclinationManual(kManualDegrees);

    CHECK(rowDeclination(model, trip) == expectedDeclination("DeclinationManual", kManualDegrees));
}
