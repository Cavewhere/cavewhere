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
#include "cwExternalCenterline.h"
#include "cwGeoReference.h"
#include "ExternalCenterlineTestHelpers.h"
#include "TestHelper.h"

// Qt
#include <QCoreApplication>

namespace {

const QString kUtm12N = QStringLiteral("EPSG:32612");
constexpr double kFirstEasting = 500000.0;
constexpr double kNorthing = 4194000.0;
constexpr double kElevation = 2700.0;
constexpr double kEastingStep = 1.0;

constexpr int kSmallOwners = 200;
constexpr int kLargeOwners = 800;
constexpr double kMaxGrowthRatio = 5.0;

//! \a count attached caves in \a region, each with one own-system fix staged
//! but not yet listed.
QList<cwCave*> addAttachedCaves(cwCavingRegion* region, int count)
{
    QList<cwCave*> caves;
    caves.reserve(count);
    for (int i = 0; i < count; ++i) {
        auto* cave = new cwCave();
        cave->setName(QStringLiteral("Cave %1").arg(i));
        region->addCave(cave);
        cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("attached.svx")));
        caves.append(cave);
    }
    REQUIRE(waitForQueuedEvents());
    return caves;
}

cwAttachedFix ownSystemFix(const cwCave* cave, int index)
{
    cwAttachedFix fix;
    fix.station = QStringLiteral("s%1").arg(index);
    fix.coordinate = QStringLiteral("%1 %2 %3")
                         .arg(kFirstEasting + index * kEastingStep, 0, 'f', 3)
                         .arg(kNorthing, 0, 'f', 3)
                         .arg(kElevation, 0, 'f', 3);
    fix.coordinateSystem = kUtm12N;
    fix.ownerId = cave->id();
    fix.fileName = QStringLiteral("attached.svx");
    return fix;
}

//! The longest event-loop gap while one turn lists a fix on each of \a count
//! attached caves, as a scan apply does for its derived records.
qint64 listFixesStall(int count)
{
    cwCavingRegion region;
    const QList<cwCave*> caves = addAttachedCaves(&region, count);
    const qint64 stall = longestMainThreadStall([&caves]() {
        for (int i = 0; i < caves.size(); ++i) {
            caves.at(i)->setAttachedFixes({ownSystemFix(caves.at(i), i)});
        }
    });
    CHECK(region.geoReference()->state() == cwGeoReference::Anchored);
    return stall;
}

} // namespace

TEST_CASE("Listing attached fixes on many owners in one turn holds the event loop in proportion to them",
          "[cwLocalProjectionManager][CoalescedPass]")
{
    const qint64 small = listFixesStall(kSmallOwners);
    const qint64 large = listFixesStall(kLargeOwners);
    INFO("longest stall: " << small << " ms at " << kSmallOwners << " owners, "
         << large << " ms at " << kLargeOwners);
    REQUIRE(small > 0);
    CHECK(static_cast<double>(large) / static_cast<double>(small) <= kMaxGrowthRatio);
}
