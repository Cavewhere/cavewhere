//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "LoadProjectHelper.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwKeyword.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwLinePlotManager.h"
#include "cwPassageTubeBuilder.h"
#include "cwPassageTubeManager.h"
#include "cwPassageTubeSettings.h"
#include "cwProject.h"
#include "cwRenderPassageTube.h"
#include "cwSignalSpy.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

//Qt includes
#include <QSettings>
#include <QTest>

//Std includes
#include <algorithm>
#include <memory>

namespace {

constexpr int kCoarseRingResolution = 8;
constexpr int kDeliveryWaitMs = 1000;
// Far past the limit in either meters or feet.
constexpr double kOutOfRangeLrud = cwPassageTubeBuilder::kMaxRadiusMeters * 10.0;

const cwKeyword kPassageType(cwKeywordModel::TypeKey, QStringLiteral("Passage"));

QList<cwPassageTubeTripVisibility*> passageProxies(const cwKeywordItemModel& keywordModel)
{
    QList<cwPassageTubeTripVisibility*> proxies;
    for (int i = 0; i < keywordModel.rowCount(); ++i) {
        cwKeywordItem* item = keywordModel.item(i);
        if (!item->keywordModel()->keywords().contains(kPassageType)) {
            continue;
        }
        if (auto* proxy = qobject_cast<cwPassageTubeTripVisibility*>(item->object())) {
            proxies.append(proxy);
        }
    }
    return proxies;
}

int tripCount(const cwCavingRegion* region)
{
    int count = 0;
    for (const cwCave* cave : region->caves()) {
        count += cave->tripCount();
    }
    return count;
}

QList<int> hiddenIndexes(const cwRenderPassageTube& tube)
{
    QList<int> hidden;
    const QVector<quint8> visibility = tube.visibility();
    for (int i = 0; i < visibility.size(); ++i) {
        if (visibility.at(i) == cwRenderPassageTube::kHidden) {
            hidden.append(i);
        }
    }
    return hidden;
}

}

TEST_CASE("cwPassageTubeManager builds the tube after a solve and filters it by trip", "[PassageTube][cwPassageTubeManager]")
{
    auto project = fileToProject(testcasesDatasetPath("network.cw"));
    cwCavingRegion* region = project->cavingRegion();
    REQUIRE(region->caveCount() == 1);

    cwPassageTubeSettings settings;
    settings.resetToDefaults();

    cwKeywordItemModel keywordModel;
    cwRenderPassageTube tube;

    auto linePlotManager = std::make_unique<cwLinePlotManager>();
    linePlotManager->setRegion(region);

    auto tubeManager = std::make_unique<cwPassageTubeManager>();
    tubeManager->setLinePlotManager(linePlotManager.get());
    tubeManager->setSettings(&settings);
    tubeManager->setRenderPassageTube(&tube);
    tubeManager->setKeywordItemModel(&keywordModel);
    tubeManager->setRegion(region);

    linePlotManager->waitToFinish();
    tubeManager->waitToFinish();

    REQUIRE_FALSE(tube.indices().isEmpty());
    REQUIRE_FALSE(tube.positions().isEmpty());
    CHECK(tube.visibility().size() == tube.indices().size());

    SECTION("one Passage keyword item per trip") {
        CHECK(passageProxies(keywordModel).size() == tripCount(region));
        CHECK(tube.tripIndexRanges().size() == tripCount(region));
    }

    SECTION("hiding a trip's keyword item hides exactly its index range") {
        const auto proxies = passageProxies(keywordModel);
        REQUIRE_FALSE(proxies.isEmpty());

        proxies.first()->setVisible(false);
        const QList<int> hidden = hiddenIndexes(tube);
        REQUIRE_FALSE(hidden.isEmpty());

        const auto ranges = tube.tripIndexRanges();
        const bool matchesOneTrip = std::any_of(ranges.cbegin(), ranges.cend(),
                                                [&](const cwPassageTubeMesh::VertexRange& range) {
            return range.first == hidden.first()
                   && range.count == hidden.size()
                   && hidden.last() == range.first + range.count - 1;
        });
        CHECK(matchesOneTrip);

        SECTION("the hidden range survives a rebuild") {
            settings.setRingResolution(kCoarseRingResolution);
            tubeManager->waitToFinish();
            CHECK(hiddenIndexes(tube).size() < hidden.size());
            CHECK_FALSE(hiddenIndexes(tube).isEmpty());

            proxies.first()->setVisible(true);
            CHECK(hiddenIndexes(tube).isEmpty());
        }
    }

    SECTION("changing the ring resolution rebuilds without a solve") {
        cwSignalSpy linePlotStateSpy(linePlotManager.get(), &cwLinePlotManager::updateStateChanged);
        cwSignalSpy positionsSpy(linePlotManager.get(), &cwLinePlotManager::stationPositionInCavesChanged);
        cwSignalSpy meshSpy(tubeManager.get(), &cwPassageTubeManager::meshChanged);

        const qsizetype fineIndexCount = tube.indices().size();
        settings.setRingResolution(kCoarseRingResolution);
        tubeManager->waitToFinish();

        CHECK(meshSpy.count() == 1);
        CHECK(tube.indices().size() < fineIndexCount);
        CHECK(linePlotStateSpy.count() == 0);
        CHECK(positionsSpy.count() == 0);
        CHECK(linePlotManager->updateState() == cwUpdatable::State::Clean);
    }

    SECTION("an out-of-range LRUD shows in warnings until it is fixed") {
        const auto outOfRangeWarnings = [&]() {
            QStringList matches;
            for (const QString& warning : tubeManager->warnings()) {
                if (warning.contains(QStringLiteral("passage dimension"))) {
                    matches.append(warning);
                }
            }
            return matches;
        };

        cwSurveyChunk* chunk = region->cave(0)->trip(0)->chunk(0);
        const cwStation original = chunk->station(0);
        CHECK(outOfRangeWarnings().isEmpty());

        cwSignalSpy warningsSpy(tubeManager.get(), &cwPassageTubeManager::warningsChanged);

        chunk->setData(cwSurveyChunk::StationLeftRole, 0, kOutOfRangeLrud);
        linePlotManager->waitToFinish();
        tubeManager->waitToFinish();

        CHECK(tubeManager->property("warnings").toStringList() == tubeManager->warnings());
        const QStringList warnings = outOfRangeWarnings();
        REQUIRE(warnings.size() == 1);
        CHECK(warnings.first().contains(original.name()));
        CHECK(warningsSpy.count() == 1);

        chunk->setStation(original, 0);
        linePlotManager->waitToFinish();
        tubeManager->waitToFinish();

        CHECK(outOfRangeWarnings().isEmpty());
        CHECK(warningsSpy.count() == 2);
    }

    SECTION("the visible flag drives the render object and persists") {
        CHECK(tube.isVisible() == tubeManager->isVisible());

        cwSignalSpy visibleSpy(tubeManager.get(), &cwPassageTubeManager::visibleChanged);
        tubeManager->setVisible(false);
        CHECK(visibleSpy.count() == 1);
        CHECK_FALSE(tube.isVisible());
        CHECK(QSettings().value(QStringLiteral("view/passageTubesVisible")).toBool() == false);

        cwPassageTubeManager reloaded;
        CHECK_FALSE(reloaded.isVisible());

        tubeManager->setVisible(true);
        CHECK(tube.isVisible());
    }

    SECTION("destroying the manager while a rebuild is debounced is safe") {
        settings.setRingResolution(kCoarseRingResolution);
        tubeManager.reset();
        QTest::qWait(cwPassageTubeManager::kRebuildDebounceMs * 2);
        CHECK(passageProxies(keywordModel).isEmpty());
    }

    SECTION("destroying the manager while the builder runs is safe") {
        const QVector<quint32> indicesBefore = tube.indices();
        cwSignalSpy meshSpy(tubeManager.get(), &cwPassageTubeManager::meshChanged);

        settings.setRingResolution(kCoarseRingResolution);
        // Long enough for the debounce to fire and hand the build to a worker.
        QTest::qWait(cwPassageTubeManager::kRebuildDebounceMs + 1);
        const bool deliveredAlready = meshSpy.count() > 0;
        tubeManager.reset();
        QTest::qWait(kDeliveryWaitMs);

        if (!deliveredAlready) {
            CHECK(tube.indices() == indicesBefore);
        }
    }

    settings.resetToDefaults();
}
