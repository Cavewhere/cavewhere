// Pins cwEntranceView: every placed fix whose station the solve placed draws one
// tip-down pin in the view's keyed sprite layer, its name above the pin in the
// keyed text layer and, for an Entrance, a length and depth line under the name
// in the stats text layer, and carries one keyword item that can hide it. In
// the scene's declutter pass ([cwLabelDeclutter]) its pins and text blocks
// claim screen space before the station labels of a participating Label3dView.
//
// The view is built in QML because its EntrancePoint tap targets come from QML.
// The window is never shown and there is no QRhi: every check reads the CPU side
// of the scene's keyed layers. Positions come from a hand-set solve, so no
// cavern run is involved.

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Qt
#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRay3D>
#include <QSignalSpy>
#include <QTextLayout>
#include <QXmlStreamReader>

// Std
#include <algorithm>
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
#include "cwLabel3dGroup.h"
#include "cwLabel3dItem.h"
#include "cwLabel3dView.h"
#include "cwLabelDeclutter.h"
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

// The one sprite slot every entrance draws, whatever its fix's Kind.
constexpr uint32_t kPinSlot = 0;

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
               "    sprites: [entrancePin]\n"
               "    Item { id: entrancePin; width: %2; height: %3 }\n"
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
    // The renderer owns the selection every marker view shares; declared before
    // the view so it outlives it.
    cwSelectionManager selection;
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
        view->setSelectionManager(&selection);
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
    CHECK(instances.at(0).spriteIndex == kPinSlot);

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

TEST_CASE("cwEntranceView draws a Well by name alone", "[cwEntranceView]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    // The positive control: as an Entrance it draws a stats line.
    CHECK(f.statsTexts()->billboardCount() == 1);

    cwFixStationModel* fixes = cave->fixStations();
    REQUIRE(fixes->setData(fixes->index(0), int(cwFixStation::Well), cwFixStationModel::KindRole));

    // The rebuild that hands the point its kind pushes the pin in the same pass.
    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);
    REQUIRE(waitUntil([&] { return point->property("kind").toInt() == int(cwFixStation::Well); }));
    CHECK(joined(f.drawnTexts()) == joined({kCaveName}));
    CHECK(f.statsTexts()->billboardCount() == 0);
    const auto instances = f.pins()->buildInstances();
    REQUIRE(instances.size() == 1);
    CHECK(instances.at(0).worldPosition == kSolvedPosition);

    REQUIRE(f.keywords.rowCount() == 1);
    const QStringList pairs = keywordPairs(f.keywords.item(0));
    CHECK(pairs.contains(QStringLiteral("Type=Well")));
    CHECK_FALSE(pairs.contains(QStringLiteral("Type=Entrance")));
}

TEST_CASE("cwEntranceView draws the one pin for a fix of every Kind", "[cwEntranceView]")
{
    const auto kind = GENERATE(cwFixStation::Entrance, cwFixStation::Well,
                               cwFixStation::RadioLocation, cwFixStation::Other);
    INFO("kind " << cwFixStation::kindName(kind).toStdString());

    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);

    cwFixStationModel* fixes = cave->fixStations();
    const bool kindWritten = fixes->setData(fixes->index(0), int(kind), cwFixStationModel::KindRole);
    CHECK((kindWritten || kind == cwFixStation::Entrance));
    REQUIRE(waitUntil([&] { return point->property("kind").toInt() == int(kind); }));

    const auto instances = f.pins()->buildInstances();
    REQUIRE(instances.size() == 1);
    CHECK(instances.at(0).spriteIndex == kPinSlot);
    CHECK(instances.at(0).worldPosition == kSolvedPosition);
}

TEST_CASE("The entrance pin carries the cave glyph and the per-kind pins are gone", "[cwEntranceView]")
{
    // The cave glyph is the one path in caveKind.svg.
    QFile caveIcon(QStringLiteral(":/icons/svg/caveKind.svg"));
    REQUIRE(caveIcon.open(QIODevice::ReadOnly));
    QXmlStreamReader reader(&caveIcon);
    QString glyph;
    while (!reader.atEnd() && glyph.isEmpty()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("path")) {
            glyph = reader.attributes().value(QStringLiteral("d")).toString();
        }
    }
    REQUIRE_FALSE(glyph.isEmpty());

    {
        QFile pin(QStringLiteral(":/icons/svg/entrancePin.svg"));
        REQUIRE(pin.open(QIODevice::ReadOnly));
        const QString pinSvg = QString::fromUtf8(pin.readAll());
        INFO(pinSvg.toStdString());
        CHECK(pinSvg.contains(glyph));
    }

    for (const QString& removed : {QStringLiteral(":/icons/svg/wellPin.svg"),
                                   QStringLiteral(":/icons/svg/radioLocationPin.svg"),
                                   QStringLiteral(":/icons/svg/otherPin.svg")}) {
        INFO(removed.toStdString());
        CHECK_FALSE(QFile::exists(removed));
    }
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

TEST_CASE("cwEntranceView takes a settable selection manager and hands it to its points",
          "[cwEntranceView]")
{
    cwSelectionManager otherSelection;
    EntranceFixture f;
    f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);

    QSignalSpy changedSpy(f.view, &cwEntranceView::selectionManagerChanged);
    f.view->setSelectionManager(&otherSelection);
    CHECK(changedSpy.count() == 1);
    CHECK(f.view->selectionManager() == &otherSelection);
    CHECK(point->property("selectionManager").value<cwSelectionManager*>() == &otherSelection);

    f.view->setSelectionManager(&otherSelection);
    CHECK(changedSpy.count() == 1);
}

TEST_CASE("cwEntranceView selects nothing without a selection manager", "[cwEntranceView]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);
    const QUuid fixId = cave->fixStations()->fixStations().at(0).id();

    f.view->setSelectionManager(nullptr);
    CHECK(f.view->selectionManager() == nullptr);

    f.view->select(cave, fixId);
    CHECK_FALSE(point->property("selected").toBool());
}

TEST_CASE("cwEntranceView clears its selected point from a manager it replaces", "[cwEntranceView]")
{
    cwSelectionManager otherSelection;
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);
    const QUuid fixId = cave->fixStations()->fixStations().at(0).id();

    f.view->select(cave, fixId);
    CHECK(f.selection.selectedItem() == point);

    f.view->setSelectionManager(&otherSelection);
    CHECK(f.selection.selectedItem() == nullptr);
    CHECK_FALSE(point->property("selected").toBool());
    CHECK(otherSelection.selectedItem() == nullptr);
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

namespace {

// Which overlay connects to the camera first, and so repositions first on a
// camera change.
enum class CameraOrder {
    LabelsFirst,
    EntrancesFirst
};

// The average label size cwLabel3dView starts from, and the margin it pads a
// candidate label with: the largest rect a short label can claim.
const QSizeF kStartingLabelSize(80.0, 14.0);
constexpr qreal kCandidateMarginFactor = 2.0;

// Pixels between two neighboring entrances: wider than a pin, narrower than a name.
constexpr qreal kNeighborPixels = 30.0;
// Pixels below an entrance's station to a station label that has room: below
// the tip-down pin and its text, and clear of a candidate label on the station.
constexpr qreal kFarPixels = 80.0;
const QVector3D kPan(2.0f, 1.0f, 0.0f);

const QString kNeighborCaveName = QStringLiteral("Calcite Hollow");
const QString kLabelOnPin = QStringLiteral("p1");
const QString kLabelOnText = QStringLiteral("t1");
const QString kLabelFar = QStringLiteral("f1");
const QString kPrivateLabel = QStringLiteral("n1");
const QString kPrivateLabelTwin = QStringLiteral("n2");

QString declutterQml()
{
    return QStringLiteral(
               "import QtQuick\n"
               "import cavewherelib\n"
               "Item {\n"
               "  width: %1; height: %1\n"
               "  Label3dView {\n"
               "    objectName: \"stationLabelView\"\n"
               "    anchors.fill: parent\n"
               "    declutterTier: LabelDeclutter.StationLabelTier\n"
               "  }\n"
               "  Label3dView {\n"
               "    objectName: \"privateLabelView\"\n"
               "    anchors.fill: parent\n"
               "  }\n"
               "  EntranceView {\n"
               "    objectName: \"entranceView\"\n"
               "    anchors.fill: parent\n"
               "    declutterTier: LabelDeclutter.EntranceTier\n"
               "    sprites: [entrancePin]\n"
               "    Item { id: entrancePin; width: %2; height: %3 }\n"
               "  }\n"
               "}\n")
        .arg(kViewportSize)
        .arg(kPinSize.width())
        .arg(kPinSize.height());
}

QRectF centeredRect(const QPointF& center, const QSizeF& size)
{
    return QRectF(center - QPointF(size.width() / 2.0, size.height() / 2.0), size);
}

// One EntranceView and two Label3dViews in one scene: "stationLabelView" joins
// the scene's declutter pass after the entrances, "privateLabelView" keeps its
// own placement.
struct DeclutterFixture
{
    cwCavingRegion region;
    cwKeywordItemModel keywords;
    cwScene scene;
    cwCamera camera;
    QQmlEngine engine;
    QQuickWindow window;
    std::unique_ptr<QObject> root;
    cwEntranceView* view = nullptr;
    cwLabel3dView* labelView = nullptr;
    cwLabel3dView* privateLabelView = nullptr;

    explicit DeclutterFixture(CameraOrder order = CameraOrder::EntrancesFirst)
    {
        camera.setViewport(QRect(0, 0, kViewportSize, kViewportSize));
        camera.setProjection(camera.orthoProjectionDefault());
        window.resize(kViewportSize, kViewportSize);

        QQmlComponent component(&engine);
        component.setData(declutterQml().toUtf8(), QUrl());
        root.reset(component.create());
        INFO(component.errorString().toStdString());
        REQUIRE(root != nullptr);
        auto* rootItem = qobject_cast<QQuickItem*>(root.get());
        REQUIRE(rootItem != nullptr);
        rootItem->setParentItem(window.contentItem());

        view = root->findChild<cwEntranceView*>(QStringLiteral("entranceView"));
        labelView = root->findChild<cwLabel3dView*>(QStringLiteral("stationLabelView"));
        privateLabelView = root->findChild<cwLabel3dView*>(QStringLiteral("privateLabelView"));
        REQUIRE(view != nullptr);
        REQUIRE(labelView != nullptr);
        REQUIRE(privateLabelView != nullptr);
        REQUIRE(view->declutterTier() == cwLabelDeclutter::EntranceTier);
        REQUIRE(labelView->declutterTier() == cwLabelDeclutter::StationLabelTier);
        REQUIRE(privateLabelView->declutterTier() == cwLabelDeclutter::NoTier);

        QFont nameFont;
        nameFont.setPixelSize(kNameFontPixels);
        view->setFont(nameFont);
        QFont statsFont;
        statsFont.setPixelSize(kStatsFontPixels);
        view->setStatsFont(statsFont);
        view->setTextGap(kTextGap);

        view->setScene(&scene);
        labelView->setScene(&scene);
        privateLabelView->setScene(&scene);

        // The camera's change signals reach the overlays in this order.
        if (order == CameraOrder::LabelsFirst) {
            labelView->setCamera(&camera);
            privateLabelView->setCamera(&camera);
            view->setCamera(&camera);
        } else {
            view->setCamera(&camera);
            labelView->setCamera(&camera);
            privateLabelView->setCamera(&camera);
        }

        view->setKeywordItemModel(&keywords);
        view->setRegion(&region);
    }

    cwCave* addEntranceCaveAt(const QString& name, const QVector3D& position)
    {
        cwCave* cave = SurveyTreeTestHelper::addNode(&region, nullptr, cwSurveyNode::Kind::Cave, name);
        cave->length()->setUnit(cwUnits::Meters);
        cave->length()->setValue(kCaveLengthMeters);
        cave->depth()->setUnit(cwUnits::Meters);
        cave->depth()->setValue(kCaveDepthMeters);
        cave->fixStations()->appendFixStation(placedFix(kStation));
        cwStationPositionLookup lookup;
        lookup.setPosition(kStation, position);
        SurveyTreeTestHelper::setScopedPositions(cave, lookup);
        REQUIRE(cave->scopedPositions().position(kStation) == position);
        return cave;
    }

    static cwLabel3dGroup* addLabels(cwLabel3dView* labelView, const QList<cwLabel3dItem>& labels)
    {
        auto* group = new cwLabel3dGroup(labelView);
        group->setLabels(labels);
        return group;
    }

    cwRenderSpriteBillboards* pins() { return scene.spriteBillboardLayer(view->layerKey()); }
    cwRenderTextBillboards* texts() { return scene.textBillboardLayer(view->layerKey()); }
    cwRenderTextBillboards* statsTexts() { return scene.textBillboardLayer(view->statsLayerKey()); }
    // Both label views draw on the scene's default text layer.
    cwRenderTextBillboards* stationTexts() { return scene.textBillboardLayer(); }

    QStringList drawnNames() { return EntranceFixture::drawnTexts(texts()); }
    QStringList drawnStats() { return EntranceFixture::drawnTexts(statsTexts()); }
    QStringList drawnStationLabels()
    {
        QStringList labels = EntranceFixture::drawnTexts(stationTexts());
        labels.sort();
        return labels;
    }
    bool isStationLabelDrawn(const QString& text) { return drawnStationLabels().contains(text); }

    int pinCount() { return int(pins()->buildInstances().size()); }

    // The world position that projects @a pixelOffset (y-down) from @a origin's
    // projection, in the plane of @a origin.
    QVector3D worldAtPixelOffset(const QVector3D& origin, const QPointF& pixelOffset) const
    {
        const QPointF base = camera.project(origin);
        const qreal pixelsPerX = camera.project(origin + QVector3D(1.0f, 0.0f, 0.0f)).x() - base.x();
        const qreal pixelsPerY = camera.project(origin + QVector3D(0.0f, 1.0f, 0.0f)).y() - base.y();
        return origin + QVector3D(float(pixelOffset.x() / pixelsPerX),
                                  float(pixelOffset.y() / pixelsPerY),
                                  0.0f);
    }

    QRectF pinRect(const QVector3D& position) const
    {
        const QPointF tip = camera.project(position);
        return QRectF(tip - QPointF(kPinSize.width() * kPinAnchor.x(), kPinSize.height() * kPinAnchor.y()),
                      kPinSize);
    }

    // The name and stats lines of an Entrance named @a name, as one block kTextGap above its pin.
    QRectF textBlockRect(const QVector3D& position, const QString& name)
    {
        const QSizeF nameSize = texts()->textSize(name);
        const QSizeF statsSize = statsTexts()->textSize(kCaveStats);
        const QPointF tip = camera.project(position);
        const qreal width = std::max(nameSize.width(), statsSize.width());
        const qreal height = nameSize.height() + statsSize.height();
        const qreal bottom = tip.y() - kPinSize.height() - kTextGap;
        return QRectF(tip.x() - width / 2.0, bottom - height, width, height);
    }

    // The station label's laid-out text, centered on its station.
    QRectF labelTextRect(const QVector3D& position, const QString& text)
    {
        return centeredRect(camera.project(position), stationTexts()->textSize(text));
    }

    // The largest rect the station label can claim: a candidate's padded rect.
    QRectF labelReachRect(const QVector3D& position, const QString& text)
    {
        const QSizeF textSize = stationTexts()->textSize(text);
        const QSizeF largest(std::max(textSize.width(), kStartingLabelSize.width()),
                             std::max(textSize.height(), kStartingLabelSize.height()));
        return centeredRect(camera.project(position), largest * kCandidateMarginFactor);
    }

    void pan()
    {
        QMatrix4x4 panned = camera.viewMatrix();
        panned.translate(kPan);
        camera.setViewMatrix(panned);
    }
};

} // namespace

TEST_CASE("A station label over an entrance pin hides and the pin stays", "[cwLabelDeclutter]")
{
    DeclutterFixture f;
    const QVector3D farPosition = f.worldAtPixelOffset(kSolvedPosition, QPointF(0.0, kFarPixels));
    DeclutterFixture::addLabels(f.labelView, {cwLabel3dItem(kLabelOnPin, kSolvedPosition),
                                              cwLabel3dItem(kLabelFar, farPosition)});

    // The positive control: with no entrance, the label has room.
    REQUIRE(waitUntil([&] { return f.isStationLabelDrawn(kLabelOnPin) && f.isStationLabelDrawn(kLabelFar); }));

    f.addEntranceCaveAt(kCaveName, kSolvedPosition);
    REQUIRE(waitUntil([&] { return f.pinCount() == 1 && f.drawnNames().size() == 1; }));
    REQUIRE(f.labelTextRect(kSolvedPosition, kLabelOnPin).intersects(f.pinRect(kSolvedPosition)));
    REQUIRE_FALSE(f.labelReachRect(farPosition, kLabelFar).intersects(f.pinRect(kSolvedPosition)));
    REQUIRE_FALSE(f.labelReachRect(farPosition, kLabelFar).intersects(f.textBlockRect(kSolvedPosition, kCaveName)));
    REQUIRE_FALSE(f.labelReachRect(farPosition, kLabelFar).intersects(f.labelReachRect(kSolvedPosition, kLabelOnPin)));

    INFO("station labels: " << joined(f.drawnStationLabels()));
    CHECK(waitUntil([&] { return !f.isStationLabelDrawn(kLabelOnPin); }));
    CHECK(f.isStationLabelDrawn(kLabelFar));
    CHECK(f.pinCount() == 1);
    CHECK(joined(f.drawnNames()) == joined({kCaveName}));
    CHECK(joined(f.drawnStats()) == joined({kCaveStats}));

    // A camera change runs the pass again with the same outcome.
    f.pan();
    CHECK_FALSE(f.isStationLabelDrawn(kLabelOnPin));
    CHECK(f.isStationLabelDrawn(kLabelFar));
    CHECK(f.pinCount() == 1);
    CHECK(joined(f.drawnNames()) == joined({kCaveName}));
}

TEST_CASE("A station label over an entrance's text hides and the text stays", "[cwLabelDeclutter]")
{
    DeclutterFixture f;
    f.addEntranceCaveAt(kCaveName, kSolvedPosition);
    REQUIRE(waitUntil([&] { return f.pinCount() == 1 && f.drawnNames().size() == 1; }));

    // Centered on the pin, just high enough that even a candidate's padded rect
    // stays clear of the pin.
    const QRectF pin = f.pinRect(kSolvedPosition);
    const QRectF block = f.textBlockRect(kSolvedPosition, kCaveName);
    const qreal reachHalfHeight = f.labelReachRect(kSolvedPosition, kLabelOnText).height() / 2.0;
    const QPointF labelPixel(0.0, pin.top() - 1.0 - reachHalfHeight - f.camera.project(kSolvedPosition).y());
    const QVector3D labelPosition = f.worldAtPixelOffset(kSolvedPosition, labelPixel);

    DeclutterFixture::addLabels(f.labelView, {cwLabel3dItem(kLabelOnText, labelPosition)});
    REQUIRE(waitForQueuedEvents());

    INFO("pin " << pin.x() << "," << pin.y() << " " << pin.width() << "x" << pin.height()
         << " block " << block.x() << "," << block.y() << " " << block.width() << "x" << block.height());
    REQUIRE_FALSE(f.labelReachRect(labelPosition, kLabelOnText).intersects(pin));
    REQUIRE(f.labelTextRect(labelPosition, kLabelOnText).intersects(block));

    INFO("station labels: " << joined(f.drawnStationLabels()));
    CHECK(waitUntil([&] { return !f.isStationLabelDrawn(kLabelOnText); }));
    CHECK(joined(f.drawnNames()) == joined({kCaveName}));
    CHECK(joined(f.drawnStats()) == joined({kCaveStats}));
    CHECK(f.pinCount() == 1);

    f.pan();
    CHECK_FALSE(f.isStationLabelDrawn(kLabelOnText));
    CHECK(joined(f.drawnNames()) == joined({kCaveName}));
    CHECK(joined(f.drawnStats()) == joined({kCaveStats}));
}

TEST_CASE("Two entrances whose text overlaps keep both pins and one text block", "[cwLabelDeclutter]")
{
    DeclutterFixture f;
    const QVector3D neighbor = f.worldAtPixelOffset(kSolvedPosition, QPointF(-kNeighborPixels, 0.0));
    f.addEntranceCaveAt(kCaveName, kSolvedPosition);
    f.addEntranceCaveAt(kNeighborCaveName, neighbor);
    REQUIRE(waitUntil([&] { return f.pinCount() == 2; }));
    REQUIRE(waitForQueuedEvents());

    // The pins clear each other and each other's text; only the text blocks meet.
    REQUIRE_FALSE(f.pinRect(kSolvedPosition).intersects(f.pinRect(neighbor)));
    REQUIRE_FALSE(f.pinRect(kSolvedPosition).intersects(f.textBlockRect(neighbor, kNeighborCaveName)));
    REQUIRE_FALSE(f.pinRect(neighbor).intersects(f.textBlockRect(kSolvedPosition, kCaveName)));
    REQUIRE(f.textBlockRect(kSolvedPosition, kCaveName).intersects(f.textBlockRect(neighbor, kNeighborCaveName)));

    INFO("names: " << joined(f.drawnNames()) << " stats: " << joined(f.drawnStats()));
    CHECK(waitUntil([&] { return f.drawnNames().size() == 1; }));
    CHECK(f.pinCount() == 2);
    const auto nameSlots = f.texts()->buildRenderSlots();
    const auto statsSlots = f.statsTexts()->buildRenderSlots();
    REQUIRE(nameSlots.size() == 1);
    REQUIRE(statsSlots.size() == 1);
    // The name and stats lines left are one entrance's block.
    CHECK(nameSlots.at(0).worldPosition == statsSlots.at(0).worldPosition);

    f.pan();
    CHECK(f.pinCount() == 2);
    CHECK(f.drawnNames().size() == 1);
    CHECK(f.drawnStats().size() == 1);

    // The entrance whose text hid keeps its tappable pin.
    const QVector3D hiddenPosition = nameSlots.at(0).worldPosition == kSolvedPosition ? neighbor
                                                                                     : kSolvedPosition;
    const QPointF tap = f.camera.project(hiddenPosition) + kOnPinBody;
    CHECK_FALSE(f.view->isOccluded(hiddenPosition, tap));
}

TEST_CASE("The declutter outcome is the same whichever overlay repositions first", "[cwLabelDeclutter]")
{
    const auto order = GENERATE(CameraOrder::LabelsFirst, CameraOrder::EntrancesFirst);
    INFO((order == CameraOrder::LabelsFirst ? "labels first" : "entrances first"));

    DeclutterFixture f(order);
    const QVector3D farPosition = f.worldAtPixelOffset(kSolvedPosition, QPointF(0.0, kFarPixels));
    DeclutterFixture::addLabels(f.labelView, {cwLabel3dItem(kLabelOnPin, kSolvedPosition),
                                              cwLabel3dItem(kLabelFar, farPosition)});
    f.addEntranceCaveAt(kCaveName, kSolvedPosition);
    REQUIRE(waitUntil([&] { return f.pinCount() == 1 && f.drawnNames().size() == 1; }));
    REQUIRE(waitForQueuedEvents());

    // One camera change, checked before any later event-loop turn: the pass it
    // ran is complete whichever overlay's slot ran first.
    f.pan();
    INFO("station labels: " << joined(f.drawnStationLabels()));
    CHECK_FALSE(f.isStationLabelDrawn(kLabelOnPin));
    CHECK(f.isStationLabelDrawn(kLabelFar));
    CHECK(f.pinCount() == 1);
    CHECK(joined(f.drawnNames()) == joined({kCaveName}));
    CHECK(joined(f.drawnStats()) == joined({kCaveStats}));
}

TEST_CASE("Entrance text shown last frame keeps its place over a newly arriving entrance",
          "[cwLabelDeclutter]")
{
    DeclutterFixture f;
    f.addEntranceCaveAt(kCaveName, kSolvedPosition);
    REQUIRE(waitUntil([&] { return f.drawnNames() == QStringList{kCaveName}; }));

    // The newcomer would win a tie-break by viewport z, then y, then x: same
    // depth and height, further left.
    const QVector3D neighbor = f.worldAtPixelOffset(kSolvedPosition, QPointF(-kNeighborPixels, 0.0));
    const QMatrix4x4 matrix = f.camera.qtViewportMatrix() * f.camera.viewProjectionMatrix();
    const QVector3D shownViewport = matrix.map(kSolvedPosition);
    const QVector3D newViewport = matrix.map(neighbor);
    REQUIRE(newViewport.z() == shownViewport.z());
    REQUIRE(qAbs(newViewport.y() - shownViewport.y()) < kOffsetTolerance);
    REQUIRE(newViewport.x() < shownViewport.x());

    f.addEntranceCaveAt(kNeighborCaveName, neighbor);
    REQUIRE(waitUntil([&] { return f.pinCount() == 2; }));
    REQUIRE(f.textBlockRect(kSolvedPosition, kCaveName).intersects(f.textBlockRect(neighbor, kNeighborCaveName)));

    INFO("names: " << joined(f.drawnNames()));
    CHECK(waitUntil([&] { return f.drawnNames() == QStringList{kCaveName}; }));
    CHECK(f.drawnStats().size() == 1);

    // Orbiting keeps the shown entrance's text in place.
    f.pan();
    CHECK(joined(f.drawnNames()) == joined({kCaveName}));
    CHECK(f.pinCount() == 2);
}

TEST_CASE("A Label3dView outside the declutter pass declutters only against itself",
          "[cwLabelDeclutter]")
{
    DeclutterFixture f;
    // Twin labels on one station: the private view shows one of them.
    DeclutterFixture::addLabels(f.privateLabelView, {cwLabel3dItem(kPrivateLabel, kSolvedPosition),
                                                     cwLabel3dItem(kPrivateLabelTwin, kSolvedPosition)});
    DeclutterFixture::addLabels(f.labelView, {cwLabel3dItem(kLabelOnPin, kSolvedPosition)});

    // The private view never sees the other view's labels, so both stations draw.
    REQUIRE(waitUntil([&] { return f.isStationLabelDrawn(kPrivateLabel) && f.isStationLabelDrawn(kLabelOnPin); }));
    REQUIRE_FALSE(f.isStationLabelDrawn(kPrivateLabelTwin));

    f.addEntranceCaveAt(kCaveName, kSolvedPosition);
    REQUIRE(waitUntil([&] { return f.pinCount() == 1 && f.drawnNames().size() == 1; }));

    INFO("station labels: " << joined(f.drawnStationLabels()));
    // The participating label yields to the pin; the private view keeps its own outcome.
    CHECK(waitUntil([&] { return !f.isStationLabelDrawn(kLabelOnPin); }));
    CHECK(f.isStationLabelDrawn(kPrivateLabel));
    CHECK_FALSE(f.isStationLabelDrawn(kPrivateLabelTwin));

    f.pan();
    CHECK_FALSE(f.isStationLabelDrawn(kLabelOnPin));
    CHECK(f.isStationLabelDrawn(kPrivateLabel));
    CHECK_FALSE(f.isStationLabelDrawn(kPrivateLabelTwin));
}
