//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwGeometryItersecter.h"
#include "cwJobSettings.h"
#include "cwPickingLog.h"
#include "cwNote.h"
#include "cwProject.h"
#include "cwRegionSceneManager.h"
#include "cwRootData.h"
#include "cwScene.h"
#include "cwScrap.h"
#include "cwScrapManager.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "LoadProjectHelper.h"

//Qt includes
#include <QLoggingCategory>
#include <QScopeGuard>

namespace {

int countScrapsInRegion(cwCavingRegion* region)
{
    int total = 0;
    if (region == nullptr) {
        return 0;
    }
    for (cwCave* cave : region->caves()) {
        if (cave == nullptr) {
            continue;
        }
        for (cwTrip* trip : cave->trips()) {
            if (trip == nullptr || trip->notes() == nullptr) {
                continue;
            }
            for (cwNote* note : trip->notes()->notes()) {
                if (note == nullptr) {
                    continue;
                }
                total += note->scraps().size();
            }
        }
    }
    return total;
}

} // namespace

TEST_CASE("Loading Phake Cave 3000 should register every scrap with the picker",
          "[cwGeometryItersecter][PhakeCave3000]")
{
    // Enable debug-level cw.picking output for this test only. Flipping the
    // category's enabled flag directly avoids clobbering any global filter
    // rules a sibling test or QT_LOGGING_RULES env var may have installed.
    const bool wasEnabled = lcPick().isDebugEnabled();
    lcPick().setEnabled(QtDebugMsg, true);
    auto restoreFilter = qScopeGuard([wasEnabled]() {
        lcPick().setEnabled(QtDebugMsg, wasEnabled);
    });

    cwJobSettings::initialize();
    REQUIRE(cwJobSettings::instance()->automaticUpdate());

    auto rootData = std::make_unique<cwRootData>();
    fileToProject(rootData->project(),
                  testcasesDatasetPath("test_cwProject/Phake Cave 3000.cw"));

    // Drain scrap triangulation, then drain the picker's async BVH build.
    // Scrap triangulation calls cwRenderTexturedItems::updateGeometry which
    // calls cwGeometryItersecter::addObject + scheduleBuild, so we must wait
    // for the scrap pipeline first and the picker second.
    rootData->scrapManager()->waitForFinish();
    rootData->futureManagerModel()->waitForFinished();

    auto* scene = rootData->regionSceneManager()->scene();
    REQUIRE(scene != nullptr);
    auto* picker = scene->geometryItersecter();
    REQUIRE(picker != nullptr);
    picker->waitForFinish();

    const int scrapCount = countScrapsInRegion(rootData->project()->cavingRegion());
    const auto stats = picker->debugStatistics();

    INFO("scrapCount=" << scrapCount
        << " hasBvh=" << stats.hasBvh
        << " sourceNodeCount=" << stats.sourceNodeCount
        << " triangleSourceNodes=" << stats.triangleSourceNodes
        << " lineSourceNodes=" << stats.lineSourceNodes
        << " pointSourceNodes=" << stats.pointSourceNodes
        << " totalPrimitives=" << stats.totalPrimitives
        << " bvhNodeCount=" << stats.bvhNodeCount);

    REQUIRE(scrapCount > 0);

    // Every scrap's triangulated geometry must land in the picker's BVH as
    // a Triangles source node. Regressions where geometry reaches the
    // renderer but not the picker (e.g. missing Type::Triangles) are
    // invisible without this check.
    REQUIRE(stats.triangleSourceNodes >= scrapCount);
}
