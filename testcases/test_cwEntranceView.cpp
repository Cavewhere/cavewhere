// Pins cwEntranceView: every placed fix whose station the solve placed draws one
// tip-down pin in the view's keyed sprite layer and one line of text above it in
// the keyed text layer, and carries one keyword item that can hide it.
//
// The view is built in QML because its EntrancePoint tap targets come from QML.
// The window is never shown and there is no QRhi: every check reads the CPU side
// of the scene's keyed layers. Positions come from a hand-set solve, so no
// cavern run is involved.

//Catch includes
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTextLayout>

// Std
#include <memory>

// SUT
#include "cwCamera.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwEntranceLabel.h"
#include "cwEntranceView.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwLength.h"
#include "cwRenderSpriteBillboards.h"
#include "cwRenderTextBillboards.h"
#include "cwScene.h"
#include "cwStationPositionLookup.h"
#include "cwVisibilityProxy.h"
#include "SurveyTreeTestHelper.h"
#include "TestHelper.h"

namespace {

constexpr int kViewportSize = 400;
const QSizeF kPinSize(16.0, 24.0);

const QString kCaveName = QStringLiteral("Gypsum Hollow");
const QString kStation = QStringLiteral("A1");
const QString kPointName = QStringLiteral("entrancePoint_A1");
constexpr double kCaveLengthMeters = 1234.0;
constexpr double kCaveDepthMeters = 85.4;

// The solved position, deliberately unrelated to the fix's input coordinate, so
// a marker drawn at the input coordinate fails the position checks.
const QVector3D kSolvedPosition(12.0f, -7.0f, 3.5f);

// A UTM 13N coordinate in Colorado, the shape test_cwFixStationDiagnostics uses.
const QString kInputCS = QStringLiteral("EPSG:32613");
constexpr double kEasting = 478000.0;
constexpr double kNorthing = 4430000.0;
constexpr double kElevation = 1655.0;

const QPointF kPinAnchor(0.5, 1.0);

// A 4x larger region may cost at most this many times the keyword inserts.
constexpr int kGrowthFactor = 4;
constexpr int kSmallRegionNodes = 4;

QString entranceViewQml()
{
    return QStringLiteral(
               "import QtQuick\n"
               "import cavewherelib\n"
               "Item {\n"
               "  width: %1; height: %1\n"
               "  EntranceView {\n"
               "    objectName: \"entranceView\"\n"
               "    anchors.fill: parent\n"
               "    sprites: [entrancePin, wellPin, radioLocationPin, otherPin]\n"
               "    Item { id: entrancePin; width: %2; height: %3 }\n"
               "    Item { id: wellPin; width: %2; height: %3 }\n"
               "    Item { id: radioLocationPin; width: %2; height: %3 }\n"
               "    Item { id: otherPin; width: %2; height: %3 }\n"
               "  }\n"
               "}\n")
        .arg(kViewportSize)
        .arg(kPinSize.width())
        .arg(kPinSize.height());
}

cwFixStation placedFix(const QString& stationName)
{
    cwFixStation fix;
    fix.setStationName(stationName);
    fix.setInputCS(kInputCS);
    fix.setCoordinate(kEasting, kNorthing, kElevation);
    return fix;
}

// Every keyword the item carries, its own and its extensions', as key=value.
QStringList keywordPairs(const cwKeywordItem* item)
{
    QStringList pairs;
    const cwKeywordModel* model = item->keywordModel();
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row);
        pairs.append(model->data(index, cwKeywordModel::KeyRole).toString()
                     + QStringLiteral("=")
                     + model->data(index, cwKeywordModel::ValueRole).toString());
    }
    return pairs;
}

// An empty region under a QML EntranceView in a window that is never shown.
struct EntranceFixture
{
    cwCavingRegion region;
    cwKeywordItemModel keywords;
    cwScene scene;
    cwCamera camera;
    QQmlEngine engine;
    QQuickWindow window;
    std::unique_ptr<QObject> root;
    cwEntranceView* view = nullptr;

    EntranceFixture()
    {
        camera.setViewport(QRect(0, 0, kViewportSize, kViewportSize));
        camera.setProjection(camera.orthoProjectionDefault());
        window.resize(kViewportSize, kViewportSize);

        QQmlComponent component(&engine);
        component.setData(entranceViewQml().toUtf8(), QUrl());
        root.reset(component.create());
        INFO(component.errorString().toStdString());
        REQUIRE(root != nullptr);
        auto* rootItem = qobject_cast<QQuickItem*>(root.get());
        REQUIRE(rootItem != nullptr);
        rootItem->setParentItem(window.contentItem());

        view = root->findChild<cwEntranceView*>(QStringLiteral("entranceView"));
        REQUIRE(view != nullptr);
        REQUIRE(view->window() == &window);
        // The view draws through its own pair of layers, never the default one
        // leads and station labels share.
        REQUIRE_FALSE(view->layerKey().isEmpty());

        view->setScene(&scene);
        view->setCamera(&camera);
        view->setKeywordItemModel(&keywords);
        view->setRegion(&region);
    }

    cwCave* addCave(const QString& name,
                    double lengthMeters = kCaveLengthMeters,
                    double depthMeters = kCaveDepthMeters)
    {
        cwCave* cave = SurveyTreeTestHelper::addNode(&region, nullptr, cwSurveyNode::Kind::Cave, name);
        cave->length()->setUnit(cwUnits::Meters);
        cave->length()->setValue(lengthMeters);
        cave->depth()->setUnit(cwUnits::Meters);
        cave->depth()->setValue(depthMeters);
        return cave;
    }

    // One cave holding one placed fix on kStation, solved at kSolvedPosition.
    cwCave* addEntranceCave(const QString& name = kCaveName)
    {
        cwCave* cave = addCave(name);
        cave->fixStations()->appendFixStation(placedFix(kStation));
        solve(cave, {{kStation, kSolvedPosition}});
        REQUIRE(cave->fixStations()->fixStations().at(0).hasPlacedCoordinate());
        REQUIRE(cave->scopedPositions().position(kStation) == kSolvedPosition);
        return cave;
    }

    void solve(cwSurveyNode* node, const QList<QPair<QString, QVector3D>>& positions)
    {
        cwStationPositionLookup lookup;
        for (const auto& position : positions) {
            lookup.setPosition(position.first, position.second);
        }
        SurveyTreeTestHelper::setScopedPositions(node, lookup);
    }

    cwRenderSpriteBillboards* pins() { return scene.spriteBillboardLayer(view->layerKey()); }
    cwRenderTextBillboards* texts() { return scene.textBillboardLayer(view->layerKey()); }

    QStringList drawnTexts()
    {
        QStringList result;
        const auto renderSlots = texts()->buildRenderSlots();
        for (const auto& slot : renderSlots) {
            result.append(slot.layout != nullptr ? slot.layout->text() : QString());
        }
        return result;
    }

    QQuickItem* point(const QString& objectName = kPointName) { return view->findChild<QQuickItem*>(objectName); }

    // Waits for the queued rebuild to draw exactly one pin and one text.
    bool waitForOneEntrance()
    {
        return waitUntil([this] {
            return pins()->buildInstances().size() == 1 && texts()->buildRenderSlots().size() == 1;
        });
    }
};

QString entranceText(const QString& name)
{
    return cwEntranceLabel::text(name, cwFixStation::Entrance,
                                 kCaveLengthMeters, kCaveDepthMeters, cwUnits::Metric);
}

} // namespace

TEST_CASE("cwEntranceView draws a placed, solved fix at its solved position", "[cwEntranceView]")
{
    EntranceFixture f;
    f.addEntranceCave();

    REQUIRE(f.waitForOneEntrance());

    const auto instances = f.pins()->buildInstances();
    CHECK(instances.at(0).worldPosition == kSolvedPosition);
    CHECK(instances.at(0).spriteIndex == uint32_t(cwFixStation::Entrance));

    const auto renderSlots = f.texts()->buildRenderSlots();
    REQUIRE(renderSlots.at(0).layout != nullptr);
    CHECK(renderSlots.at(0).layout->text() == entranceText(kCaveName));
    CHECK(renderSlots.at(0).layout->text().startsWith(kCaveName));
    CHECK(renderSlots.at(0).worldPosition == kSolvedPosition);

    // The pin stands on the station tip-down, and the text sits above the pin.
    CHECK(f.pins()->anchor() == kPinAnchor);
    CHECK(f.view->spriteSize() == kPinSize);
    CHECK(renderSlots.at(0).pixelOffset.y() > f.view->spriteSize().height());

    CHECK(f.view->entranceCount() == 1);

    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);
    CHECK(point->property("position3D").value<QVector3D>() == kSolvedPosition);
    CHECK(point->property("stationName").toString() == kStation);
}

TEST_CASE("cwEntranceView names a fix by its label, else its node", "[cwEntranceView]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());

    CHECK(f.drawnTexts() == QStringList{entranceText(kCaveName)});

    const QString label = QStringLiteral("North Shaft");
    cwFixStationModel* fixes = cave->fixStations();
    REQUIRE(fixes->setData(fixes->index(0), label, cwFixStationModel::LabelRole));

    REQUIRE(waitUntil([&] { return f.drawnTexts() == QStringList{entranceText(label)}; }));
    CHECK(f.drawnTexts().at(0).startsWith(label));

    REQUIRE(f.keywords.rowCount() == 1);
    CHECK(keywordPairs(f.keywords.item(0)).contains(QStringLiteral("Name=North Shaft")));

    // Clearing the label falls back to the node's name again.
    REQUIRE(fixes->setData(fixes->index(0), QString(), cwFixStationModel::LabelRole));
    REQUIRE(waitUntil([&] { return f.drawnTexts() == QStringList{entranceText(kCaveName)}; }));
}

TEST_CASE("cwEntranceView draws nothing for a fix the solve missed or that has no coordinate",
          "[cwEntranceView]")
{
    EntranceFixture f;
    cwCave* cave = f.addCave(kCaveName);

    const QString unsolved = QStringLiteral("B1");
    const QString empty = QStringLiteral("C1");
    const QVector3D emptyPosition(-40.0f, 2.0f, 0.0f);

    cwFixStation emptyFix;
    emptyFix.setStationName(empty);
    REQUIRE(emptyFix.state() == cwFixStation::Empty);

    cave->fixStations()->appendFixStation(placedFix(kStation));
    cave->fixStations()->appendFixStation(placedFix(unsolved));
    cave->fixStations()->appendFixStation(emptyFix);
    f.solve(cave, {{kStation, kSolvedPosition}, {empty, emptyPosition}});
    REQUIRE(cave->scopedPositions().hasPosition(empty));
    REQUIRE_FALSE(cave->scopedPositions().hasPosition(unsolved));

    // The placed, solved fix is the positive control: the pass ran and drew it.
    REQUIRE(f.waitForOneEntrance());
    REQUIRE(waitForQueuedEvents());

    const auto instances = f.pins()->buildInstances();
    REQUIRE(instances.size() == 1);
    CHECK(instances.at(0).worldPosition == kSolvedPosition);
    CHECK(f.texts()->buildRenderSlots().size() == 1);
    CHECK(f.pins()->billboardCount() == 1);
    CHECK(f.view->entranceCount() == 1);
    CHECK(f.keywords.rowCount() == 1);

    CHECK(f.point() != nullptr);
    CHECK(f.point(QStringLiteral("entrancePoint_B1")) == nullptr);
    CHECK(f.point(QStringLiteral("entrancePoint_C1")) == nullptr);
}

TEST_CASE("cwEntranceView draws a Well by name alone in the Well slot", "[cwEntranceView]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    REQUIRE(f.pins()->buildInstances().at(0).spriteIndex == uint32_t(cwFixStation::Entrance));

    cwFixStationModel* fixes = cave->fixStations();
    REQUIRE(fixes->setData(fixes->index(0), int(cwFixStation::Well), cwFixStationModel::KindRole));

    REQUIRE(waitUntil([&] { return f.drawnTexts() == QStringList{kCaveName}; }));
    const auto instances = f.pins()->buildInstances();
    REQUIRE(instances.size() == 1);
    CHECK(instances.at(0).spriteIndex == uint32_t(cwFixStation::Well));
    CHECK(instances.at(0).worldPosition == kSolvedPosition);

    REQUIRE(f.keywords.rowCount() == 1);
    const QStringList pairs = keywordPairs(f.keywords.item(0));
    CHECK(pairs.contains(QStringLiteral("Type=Well")));
    CHECK_FALSE(pairs.contains(QStringLiteral("Type=Entrance")));
}

TEST_CASE("cwEntranceView drops the pin, text and keyword item of a removed fix", "[cwEntranceView]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    REQUIRE(f.keywords.rowCount() == 1);

    cave->fixStations()->removeAt(0);

    REQUIRE(waitUntil([&] {
        return f.pins()->billboardCount() == 0
               && f.texts()->billboardCount() == 0
               && f.keywords.rowCount() == 0
               && f.point() == nullptr;
    }));
    CHECK(f.view->entranceCount() == 0);
}

TEST_CASE("cwEntranceView's keyword item names the fix and hides it", "[cwEntranceView]")
{
    EntranceFixture f;
    f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());

    REQUIRE(f.keywords.rowCount() == 1);
    cwKeywordItem* item = f.keywords.item(0);
    REQUIRE(item != nullptr);

    const QStringList pairs = keywordPairs(item);
    INFO("keywords: " << pairs.join(QStringLiteral(", ")).toStdString());
    CHECK(pairs.contains(QStringLiteral("Type=Entrance")));
    CHECK(pairs.contains(QStringLiteral("Name=") + kCaveName));
    // It extends the owning node's keywords, so a cave filter reaches it too.
    CHECK(pairs.contains(QStringLiteral("Cave=") + kCaveName));

    auto* proxy = qobject_cast<cwVisibilityProxy*>(item->object());
    REQUIRE(proxy != nullptr);
    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);
    REQUIRE(point->isVisible());

    proxy->setVisible(false);
    REQUIRE(waitUntil([&] {
        return f.pins()->buildInstances().isEmpty() && f.texts()->buildRenderSlots().isEmpty();
    }));
    CHECK_FALSE(point->isVisible());
    // Hidden is not removed: the count of drawn entrances is the drawn set.
    CHECK(f.view->entranceCount() == 1);
    CHECK(f.keywords.rowCount() == 1);

    proxy->setVisible(true);
    REQUIRE(f.waitForOneEntrance());
    CHECK(point->isVisible());
    CHECK(f.drawnTexts() == QStringList{entranceText(kCaveName)});
}

namespace {

struct BurstCounts {
    int keywordRowsInserted = 0;
    int entrancesChanged = 0;
};

// Adds @a nodeCount caves with one placed, solved fix each in one event-loop
// turn, then counts what the view emitted for that burst.
BurstCounts addEntranceBurst(int nodeCount)
{
    EntranceFixture f;
    REQUIRE(waitForQueuedEvents());

    QSignalSpy rowsInsertedSpy(&f.keywords, &QAbstractItemModel::rowsInserted);
    QSignalSpy entrancesChangedSpy(f.view, &cwEntranceView::entrancesChanged);

    for (int i = 0; i < nodeCount; ++i) {
        f.addEntranceCave(QStringLiteral("Cave %1").arg(i + 1));
    }

    INFO("nodes " << nodeCount);
    REQUIRE(waitUntil([&] {
        return f.view->entranceCount() == nodeCount && f.keywords.rowCount() == nodeCount;
    }));
    REQUIRE(waitForQueuedEvents());
    CHECK(f.pins()->buildInstances().size() == nodeCount);

    return {int(rowsInsertedSpy.count()), int(entrancesChangedSpy.count())};
}

} // namespace

TEST_CASE("cwEntranceView rebuilds once per burst and grows its keyword inserts linearly",
          "[cwEntranceView]")
{
    const BurstCounts small = addEntranceBurst(kSmallRegionNodes);
    const BurstCounts large = addEntranceBurst(kSmallRegionNodes * kGrowthFactor);

    INFO("keyword rowsInserted small " << small.keywordRowsInserted
         << " large " << large.keywordRowsInserted);
    CHECK(small.keywordRowsInserted >= 1);
    CHECK(large.keywordRowsInserted <= kGrowthFactor * small.keywordRowsInserted);

    CHECK(small.entrancesChanged == 1);
    CHECK(large.entrancesChanged == 1);
}
