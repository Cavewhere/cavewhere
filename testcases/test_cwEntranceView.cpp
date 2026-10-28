// Pins cwEntranceView: every placed fix whose station the solve placed draws one
// tip-down pin in the view's keyed sprite layer, its name above the pin in the
// keyed text layer and, for an Entrance, a length and depth line under the name
// in the stats text layer, and carries one keyword item that can hide it.
//
// The view is built in QML because its EntrancePoint tap targets come from QML.
// The window is never shown and there is no QRhi: every check reads the CPU side
// of the scene's keyed layers. Positions come from a hand-set solve, so no
// cavern run is involved.

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Qt
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRay3D>
#include <QSignalSpy>
#include <QTextLayout>

// Std
#include <memory>

// SUT
#include "cwCamera.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwEntranceView.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeometryItersecter.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwLength.h"
#include "cwRenderSpriteBillboards.h"
#include "cwRenderTextBillboards.h"
#include "cwScene.h"
#include "cwSelectionManager.h"
#include "cwStationPositionLookup.h"
#include "cwVisibilityProxy.h"
#include "SurveyTreeTestHelper.h"
#include "TestGeometryBuilders.h"
#include "TestHelper.h"

namespace {

constexpr int kViewportSize = 400;
const QSizeF kPinSize(16.0, 24.0);

const QString kCaveName = QStringLiteral("Gypsum Hollow");
const QString kStation = QStringLiteral("A1");
const QString kPointName = QStringLiteral("entrancePoint_A1");
constexpr double kCaveLengthMeters = 1234.0;
constexpr double kCaveDepthMeters = 85.4;
// The stats line cwEntranceLabel gives kCaveLengthMeters and kCaveDepthMeters in metric.
const QString kCaveStats = QStringLiteral("\u21921.2 km  \u219385 m");
constexpr double kLongerCaveLengthMeters = 2500.0;
const QString kLongerCaveStats = QStringLiteral("\u21922.5 km  \u219385 m");

// The solved position, deliberately unrelated to the fix's input coordinate, so
// a marker drawn at the input coordinate fails the position checks.
const QVector3D kSolvedPosition(12.0f, -7.0f, 3.5f);

// A UTM 13N coordinate in Colorado, the shape test_cwFixStationDiagnostics uses.
const QString kInputCS = QStringLiteral("EPSG:32613");
constexpr double kEasting = 478000.0;
constexpr double kNorthing = 4430000.0;
constexpr double kElevation = 1655.0;

const QPointF kPinAnchor(0.5, 1.0);

// Logical pixels between the pin and the text block above it.
constexpr qreal kTextGap = 3.0;
constexpr qreal kOffsetTolerance = 1e-6;

constexpr int kNameFontPixels = 18;
constexpr int kStatsFontPixels = 14;
constexpr int kLargerStatsFontPixels = 28;
const QColor kNameColor(250, 250, 250);
const QColor kNameOutlineColor(20, 20, 20);
const QColor kStatsColor(85, 85, 85);
const QColor kStatsOutlineColor(240, 240, 240);
const QColor kOtherStatsColor(201, 201, 201);

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
    // The name lines.
    cwRenderTextBillboards* texts() { return scene.textBillboardLayer(view->layerKey()); }
    // The length and depth lines under the names.
    cwRenderTextBillboards* statsTexts() { return scene.textBillboardLayer(view->statsLayerKey()); }

    static QStringList drawnTexts(const cwRenderTextBillboards* layer)
    {
        QStringList result;
        const auto renderSlots = layer->buildRenderSlots();
        for (const auto& slot : renderSlots) {
            result.append(slot.layout != nullptr ? slot.layout->text() : QString());
        }
        return result;
    }

    QStringList drawnTexts() { return drawnTexts(texts()); }
    QStringList drawnStatsTexts() { return drawnTexts(statsTexts()); }

    QQuickItem* point(const QString& objectName = kPointName) { return view->findChild<QQuickItem*>(objectName); }

    // Waits for the queued rebuild to draw exactly one pin and one name. The
    // same pass pushes the stats line, so it is there once this returns true.
    bool waitForOneEntrance()
    {
        return waitUntil([this] {
            return pins()->buildInstances().size() == 1 && texts()->buildRenderSlots().size() == 1;
        });
    }
};

// std::string, so a failure prints the drawn lines.
std::string joined(const QStringList& lines)
{
    return lines.join(QStringLiteral(" | ")).toStdString();
}

} // namespace

TEST_CASE("cwEntranceView draws a placed, solved fix at its solved position", "[cwEntranceView]")
{
    EntranceFixture f;
    f.view->setTextGap(kTextGap);
    f.addEntranceCave();

    REQUIRE(f.waitForOneEntrance());

    const auto instances = f.pins()->buildInstances();
    CHECK(instances.at(0).worldPosition == kSolvedPosition);
    CHECK(instances.at(0).spriteIndex == uint32_t(cwFixStation::Entrance));

    // The name and the stats lines live on two layers, each with its own style.
    REQUIRE(f.statsTexts() != f.texts());
    CHECK(f.statsTexts()->window() == &f.window);

    const auto nameSlots = f.texts()->buildRenderSlots();
    REQUIRE(nameSlots.size() == 1);
    REQUIRE(nameSlots.at(0).layout != nullptr);
    CHECK(nameSlots.at(0).layout->text().toStdString() == kCaveName.toStdString());
    CHECK(nameSlots.at(0).worldPosition == kSolvedPosition);

    const auto statsSlots = f.statsTexts()->buildRenderSlots();
    REQUIRE(statsSlots.size() == 1);
    REQUIRE(statsSlots.at(0).layout != nullptr);
    CHECK(statsSlots.at(0).layout->text().toStdString() == kCaveStats.toStdString());
    CHECK(statsSlots.at(0).worldPosition == kSolvedPosition);

    // The pin stands on the station tip-down. The text block sits kTextGap above
    // the pin: the stats line at its bottom, the name directly above it. Each
    // line centers on its offset point, y-up.
    CHECK(f.pins()->anchor() == kPinAnchor);
    CHECK(f.view->spriteSize() == kPinSize);
    const qreal nameHeight = f.texts()->textSize(kCaveName).height();
    const qreal statsHeight = f.statsTexts()->textSize(kCaveStats).height();
    REQUIRE(nameHeight > 0.0);
    REQUIRE(statsHeight > 0.0);

    const QPointF nameOffset = nameSlots.at(0).pixelOffset;
    const QPointF statsOffset = statsSlots.at(0).pixelOffset;
    CHECK(nameOffset.x() == 0.0);
    CHECK(statsOffset.x() == 0.0);
    CHECK_THAT(statsOffset.y(),
               Catch::Matchers::WithinAbs(kPinSize.height() + kTextGap + statsHeight / 2.0,
                                          kOffsetTolerance));
    CHECK_THAT(nameOffset.y() - statsOffset.y(),
               Catch::Matchers::WithinAbs((nameHeight + statsHeight) / 2.0, kOffsetTolerance));

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

    CHECK(joined(f.drawnTexts()) == joined({kCaveName}));
    CHECK(joined(f.drawnStatsTexts()) == joined({kCaveStats}));

    const QString label = QStringLiteral("North Shaft");
    cwFixStationModel* fixes = cave->fixStations();
    QSignalSpy entrancesChangedSpy(f.view, &cwEntranceView::entrancesChanged);
    REQUIRE(fixes->setData(fixes->index(0), label, cwFixStationModel::LabelRole));

    REQUIRE(waitUntil([&] { return f.drawnTexts() == QStringList{label}; }));
    CHECK(entrancesChangedSpy.count() == 1);
    // The label names the fix; the stats line stays the cave's.
    CHECK(joined(f.drawnStatsTexts()) == joined({kCaveStats}));

    REQUIRE(f.keywords.rowCount() == 1);
    CHECK(keywordPairs(f.keywords.item(0)).contains(QStringLiteral("Name=North Shaft")));

    // Clearing the label falls back to the node's name again.
    REQUIRE(fixes->setData(fixes->index(0), QString(), cwFixStationModel::LabelRole));
    REQUIRE(waitUntil([&] { return f.drawnTexts() == QStringList{kCaveName}; }));
}

TEST_CASE("cwEntranceView redraws the stats line when the cave's length changes", "[cwEntranceView]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    CHECK(joined(f.drawnStatsTexts()) == joined({kCaveStats}));

    QSignalSpy entrancesChangedSpy(f.view, &cwEntranceView::entrancesChanged);
    cave->length()->setValue(kLongerCaveLengthMeters);

    REQUIRE(waitUntil([&] { return f.drawnStatsTexts() == QStringList{kLongerCaveStats}; }));
    CHECK(joined(f.drawnTexts()) == joined({kCaveName}));
    CHECK(entrancesChangedSpy.count() == 1);
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
    CHECK(f.statsTexts()->buildRenderSlots().size() == 1);
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
    // The positive control: as an Entrance it draws a stats line.
    CHECK(f.statsTexts()->billboardCount() == 1);

    cwFixStationModel* fixes = cave->fixStations();
    REQUIRE(fixes->setData(fixes->index(0), int(cwFixStation::Well), cwFixStationModel::KindRole));

    REQUIRE(waitUntil([&] { return f.pins()->buildInstances().size() == 1
                                   && f.pins()->buildInstances().at(0).spriteIndex
                                          == uint32_t(cwFixStation::Well); }));
    CHECK(joined(f.drawnTexts()) == joined({kCaveName}));
    CHECK(f.statsTexts()->billboardCount() == 0);
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
    CHECK(f.statsTexts()->billboardCount() == 1);

    cave->fixStations()->removeAt(0);

    REQUIRE(waitUntil([&] {
        return f.pins()->billboardCount() == 0
               && f.texts()->billboardCount() == 0
               && f.statsTexts()->billboardCount() == 0
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
    CHECK(joined(f.drawnStatsTexts()) == joined({kCaveStats}));

    proxy->setVisible(false);
    REQUIRE(waitUntil([&] {
        return f.pins()->buildInstances().isEmpty()
               && f.texts()->buildRenderSlots().isEmpty()
               && f.statsTexts()->buildRenderSlots().isEmpty();
    }));
    CHECK_FALSE(point->isVisible());
    // Hidden is not removed: the count of drawn entrances is the drawn set.
    CHECK(f.view->entranceCount() == 1);
    CHECK(f.keywords.rowCount() == 1);

    proxy->setVisible(true);
    REQUIRE(f.waitForOneEntrance());
    CHECK(point->isVisible());
    CHECK(joined(f.drawnTexts()) == joined({kCaveName}));
    CHECK(joined(f.drawnStatsTexts()) == joined({kCaveStats}));
}

TEST_CASE("cwEntranceView styles the name and stats layers from its properties", "[cwEntranceView]")
{
    EntranceFixture f;
    f.view->setTextGap(kTextGap);

    QFont nameFont;
    nameFont.setPixelSize(kNameFontPixels);
    f.view->setFont(nameFont);
    f.view->setColor(kNameColor);
    f.view->setOutlineColor(kNameOutlineColor);

    QFont statsFont;
    statsFont.setPixelSize(kStatsFontPixels);
    QSignalSpy statsFontSpy(f.view, &cwEntranceView::statsFontChanged);
    QSignalSpy statsColorSpy(f.view, &cwEntranceView::statsColorChanged);
    QSignalSpy statsOutlineColorSpy(f.view, &cwEntranceView::statsOutlineColorChanged);
    f.view->setStatsFont(statsFont);
    f.view->setStatsColor(kStatsColor);
    f.view->setStatsOutlineColor(kStatsOutlineColor);
    CHECK(statsFontSpy.count() == 1);
    CHECK(statsColorSpy.count() == 1);
    CHECK(statsOutlineColorSpy.count() == 1);
    CHECK(f.view->statsFont().pixelSize() == kStatsFontPixels);
    CHECK(f.view->statsColor() == kStatsColor);
    CHECK(f.view->statsOutlineColor() == kStatsOutlineColor);

    f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    REQUIRE(f.statsTexts() != f.texts());

    const cwRenderTextBillboards::Style nameStyle = f.texts()->style();
    CHECK(nameStyle.font.pixelSize() == kNameFontPixels);
    CHECK(nameStyle.color.name().toStdString() == kNameColor.name().toStdString());
    CHECK(nameStyle.outlineColor.name().toStdString() == kNameOutlineColor.name().toStdString());

    const cwRenderTextBillboards::Style statsStyle = f.statsTexts()->style();
    CHECK(statsStyle.font.pixelSize() == kStatsFontPixels);
    CHECK(statsStyle.color.name().toStdString() == kStatsColor.name().toStdString());
    CHECK(statsStyle.outlineColor.name().toStdString() == kStatsOutlineColor.name().toStdString());

    SECTION("A stats color change reaches the stats layer alone")
    {
        f.view->setStatsColor(kOtherStatsColor);
        CHECK(f.statsTexts()->style().color.name().toStdString() == kOtherStatsColor.name().toStdString());
        CHECK(f.texts()->style().color.name().toStdString() == kNameColor.name().toStdString());
    }

    SECTION("A larger stats font lifts the name to stay directly above the stats line")
    {
        const auto before = f.texts()->buildRenderSlots();
        REQUIRE(before.size() == 1);
        const qreal nameLiftBefore = before.at(0).pixelOffset.y();

        QFont largerStatsFont;
        largerStatsFont.setPixelSize(kLargerStatsFontPixels);
        f.view->setStatsFont(largerStatsFont);
        CHECK(f.statsTexts()->style().font.pixelSize() == kLargerStatsFontPixels);

        const auto nameSlots = f.texts()->buildRenderSlots();
        const auto statsSlots = f.statsTexts()->buildRenderSlots();
        REQUIRE(nameSlots.size() == 1);
        REQUIRE(statsSlots.size() == 1);
        CHECK(nameSlots.at(0).pixelOffset.y() > nameLiftBefore);

        const qreal nameHeight = f.texts()->textSize(kCaveName).height();
        const qreal statsHeight = f.statsTexts()->textSize(kCaveStats).height();
        CHECK_THAT(statsSlots.at(0).pixelOffset.y(),
                   Catch::Matchers::WithinAbs(kPinSize.height() + kTextGap + statsHeight / 2.0,
                                              kOffsetTolerance));
        CHECK_THAT(nameSlots.at(0).pixelOffset.y() - statsSlots.at(0).pixelOffset.y(),
                   Catch::Matchers::WithinAbs((nameHeight + statsHeight) / 2.0, kOffsetTolerance));
    }
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

namespace {

// Whole-pixel tap offsets from the station's projection (the occlusion ray
// rounds to a pixel). With the 24 px pin standing tip-down on the station,
// kOnPinBody is on the pin; kBelowTip is where a centered sprite would draw its
// lower half and the pin draws nothing.
const QPointF kOnPinBody(0.0, -18.0);
const QPointF kBelowTip(0.0, 6.0);

constexpr uint64_t kWallId = 1;
constexpr float kWallInFront = 50.0f;
constexpr float kWallHalfExtent = 30.0f;
// Off-center so the tested ray doesn't run along the quad's shared diagonal.
const QVector3D kWallCenterOffset(5.0f, 3.0f, 0.0f);

// A camera-facing wall kWallInFront nearer the eye (the default camera looks
// down -Z) than the solved station, centered near @a tap's ray.
void addWallInFront(EntranceFixture& f, const QPointF& tap)
{
    const QRay3D ray = f.camera.rayFromQtViewport(tap);
    const QVector3D center = ray.point(ray.origin().z() - kSolvedPosition.z()) + kWallCenterOffset
                             + QVector3D(0.0f, 0.0f, kWallInFront);
    f.scene.geometryItersecter()->addObject(cwGeometryItersecter::Object(
        nullptr, kWallId, cwTestGeometry::zPlaneQuad(center, kWallHalfExtent)));
    f.scene.geometryItersecter()->waitForFinish();
}

} // namespace

TEST_CASE("cwEntranceView tests a tap against its tip-down pin", "[cwEntranceView]")
{
    EntranceFixture f;
    f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    REQUIRE(f.view->spriteSize() == kPinSize);

    const QPointF station = f.camera.project(kSolvedPosition);
    CHECK_FALSE(f.view->isOccluded(kSolvedPosition, station + kOnPinBody));
    CHECK(f.view->isOccluded(kSolvedPosition, station + kBelowTip));
}

TEST_CASE("cwEntranceView reports a pin behind a wall as occluded", "[cwEntranceView]")
{
    EntranceFixture f;
    f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());

    const QPointF tap = f.camera.project(kSolvedPosition) + kOnPinBody;
    REQUIRE_FALSE(f.view->isOccluded(kSolvedPosition, tap));

    addWallInFront(f, tap);
    CHECK(f.view->isOccluded(kSolvedPosition, tap));
}

TEST_CASE("cwEntranceView selects a fix's point by its node and id", "[cwEntranceView]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    cwCave* otherCave = f.addCave(QStringLiteral("Other Cave"));
    REQUIRE(f.waitForOneEntrance());
    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);
    const QUuid fixId = cave->fixStations()->fixStations().at(0).id();

    f.view->select(otherCave, fixId);
    CHECK(f.view->selectionManager()->selectedItem() == nullptr);

    f.view->select(cave, fixId);
    CHECK(f.view->selectionManager()->selectedItem() == point);
    CHECK(point->property("selected").toBool());
}

TEST_CASE("cwEntranceView hands each point its display name and kind", "[cwEntranceView]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);

    CHECK(point->property("displayName").toString() == kCaveName);
    CHECK(point->property("kind").toInt() == int(cwFixStation::Entrance));
    CHECK(point->property("kindName").toString() == cwFixStation::kindName(cwFixStation::Entrance));

    cwFixStationModel* fixes = cave->fixStations();
    const QString label = QStringLiteral("North Shaft");
    REQUIRE(fixes->setData(fixes->index(0), label, cwFixStationModel::LabelRole));
    REQUIRE(fixes->setData(fixes->index(0), int(cwFixStation::Well), cwFixStationModel::KindRole));

    CHECK(waitUntil([&] { return point->property("kind").toInt() == int(cwFixStation::Well); }));
    CHECK(point->property("kindName").toString() == cwFixStation::kindName(cwFixStation::Well));
    CHECK(point->property("displayName").toString() == label);
}
