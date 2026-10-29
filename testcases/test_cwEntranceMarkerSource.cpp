// Pins the entrance markers, a cwMarkerView fed by a cwEntranceMarkerSource:
// every entrance the last solve flagged, fixed or not, draws one tip-down pin
// at its solved position in the view's keyed sprite layer, its label or its
// cave's name above the pin in the keyed text layer ("name" style) and the
// cave's length and depth line under the name in the stats text layer ("stats"
// style), and carries one keyword item that can hide it. A fix on a station
// the solve did not flag draws nothing. In the scene's declutter pass
// ([cwLabelDeclutter]) its pins and text blocks claim screen space before the
// station labels of a participating Label3dView, as long as the entrance draws:
// one the depth test hides behind a wall, with x-ray off, claims nothing. A tap
// on a pin behind a wall selects it while the pin draws as a ghost or over
// everything. The ghost's opacity ramps with the camera's tilt from the base
// knob (level) to the overhead knob (straight down), and the view reads it out.
//
// The view is built in QML because its EntrancePoint tap targets come from QML.
// The window is never shown and there is no QRhi: every check reads the CPU side
// of the scene's keyed layers. Solved entrances come from a hand-set solve,
// except for the one case that runs cavern through cwLinePlotManager.

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

// Qt
#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlListReference>
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
#include "cwEntranceMarkerSource.h"
#include "cwEntranceModel.h"
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
#include "cwLinePlotManager.h"
#include "cwMarkerSource.h"
#include "cwMarkerTextStyle.h"
#include "cwMarkerView.h"
#include "cwMarkerKnobsReset.h"
#include "cwRenderingSettings.h"
#include "cwRenderSpriteBillboards.h"
#include "cwRenderTextBillboards.h"
#include "cwScene.h"
#include "cwSelectionManager.h"
#include "cwSolvedEntrance.h"
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

// The one sprite slot every entrance draws.
constexpr uint32_t kPinSlot = 0;

// The layer keys and text style keys GLTerrainRenderer gives the entrance view.
const QString kEntranceLayerKey = QStringLiteral("entrances");
const QString kStatsLayerKey = QStringLiteral("entranceStats");

// A 4x larger region may cost at most this many times the keyword inserts.
constexpr int kGrowthFactor = 4;
constexpr int kSmallRegionNodes = 4;

// The entrance MarkerView's body, as GLTerrainRenderer declares it: the
// "entrances" layers, the entrance source and delegate, and the name style on
// the view's own text layer with the stats style on its own layer.
QString entranceMarkerViewBody()
{
    return QStringLiteral(
               "    objectName: \"entranceView\"\n"
               "    anchors.fill: parent\n"
               "    layerKey: \"%1\"\n"
               "    sprites: [entrancePin]\n"
               "    source: EntranceMarkerSource {}\n"
               "    delegate: EntrancePoint {}\n"
               "    textStyles: [\n"
               "      MarkerTextStyle { styleKey: \"name\" },\n"
               "      MarkerTextStyle { styleKey: \"stats\"; layerKey: \"%2\" }\n"
               "    ]\n"
               "    Item { id: entrancePin; width: %3; height: %4 }\n")
        .arg(kEntranceLayerKey)
        .arg(kStatsLayerKey)
        .arg(kPinSize.width())
        .arg(kPinSize.height());
}

QString entranceViewQml()
{
    return QStringLiteral(
               "import QtQuick\n"
               "import cavewherelib\n"
               "Item {\n"
               "  width: %1; height: %1\n"
               "  MarkerView {\n"
               "%2"
               "  }\n"
               "}\n")
        .arg(kViewportSize)
        .arg(entranceMarkerViewBody());
}

// The view's text style @a index: 0 styles the names, 1 the stats lines.
cwMarkerTextStyle* textStyle(cwMarkerView* view, int index)
{
    QQmlListReference styles(view, "textStyles");
    if (index >= styles.count()) {
        return nullptr;
    }
    return qobject_cast<cwMarkerTextStyle*>(styles.at(index));
}

// What a solve reports for an entrance on @a stationName at @a position.
cwSolvedEntrance solvedEntrance(const QString& stationName, const QVector3D& position, bool fixed)
{
    cwSolvedEntrance entrance;
    entrance.stationName = stationName;
    entrance.qualifiedName = stationName.toLower();
    entrance.position = position;
    entrance.fixed = fixed;
    return entrance;
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

// An empty region under a QML entrance MarkerView in a window that is never shown.
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
    cwMarkerView* view = nullptr;
    cwEntranceMarkerSource* source = nullptr;
    cwMarkerTextStyle* nameStyle = nullptr;
    cwMarkerTextStyle* statsStyle = nullptr;

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

        view = root->findChild<cwMarkerView*>(QStringLiteral("entranceView"));
        REQUIRE(view != nullptr);
        REQUIRE(view->window() == &window);
        // The view draws through its own pair of layers, never the default one
        // leads and station labels share.
        REQUIRE_FALSE(view->layerKey().isEmpty());
        source = qobject_cast<cwEntranceMarkerSource*>(view->source());
        REQUIRE(source != nullptr);
        nameStyle = textStyle(view, 0);
        statsStyle = textStyle(view, 1);
        REQUIRE(nameStyle != nullptr);
        REQUIRE(statsStyle != nullptr);

        view->setScene(&scene);
        view->setCamera(&camera);
        view->setKeywordItemModel(&keywords);
        view->setSelectionManager(&selection);
        source->setRegion(&region);
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

    // One cave holding one placed fix on kStation that the solve placed at
    // kSolvedPosition and flagged as a fixed entrance.
    cwCave* addEntranceCave(const QString& name = kCaveName)
    {
        cwCave* cave = addCave(name);
        cave->fixStations()->appendFixStation(placedFix(kStation));
        solve(cave, {{kStation, kSolvedPosition}});
        SurveyTreeTestHelper::setSolvedEntrances(cave, {solvedEntrance(kStation, kSolvedPosition, true)});
        REQUIRE(cave->fixStations()->fixStations().at(0).hasPlacedCoordinate());
        REQUIRE(cave->scopedPositions().position(kStation) == kSolvedPosition);
        return cave;
    }

    // One cave with no fix whose solve flagged kStation an unfixed entrance at
    // @a position, the way a native entrance on an unfixed survey solves.
    cwCave* addUnfixedEntranceCave(const QString& name = kCaveName,
                                   const QVector3D& position = kSolvedPosition)
    {
        cwCave* cave = addCave(name);
        SurveyTreeTestHelper::setSolvedEntrances(cave, {solvedEntrance(kStation, position, false)});
        REQUIRE(cave->fixStations()->rowCount() == 0);
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
    cwRenderTextBillboards* statsTexts() { return scene.textBillboardLayer(kStatsLayerKey); }

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

TEST_CASE("The entrance MarkerView draws a placed, solved fix at its solved position", "[cwEntranceMarkerSource]")
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

    CHECK(f.view->count() == 1);
    CHECK(f.source->count() == 1);

    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);
    CHECK(point->property("position3D").value<QVector3D>() == kSolvedPosition);
    CHECK(point->property("stationName").toString() == kStation);
}

TEST_CASE("The entrance MarkerView names an entrance by its entrance label, else its cave", "[cwEntranceMarkerSource]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());

    CHECK(joined(f.drawnTexts()) == joined({kCaveName}));
    CHECK(joined(f.drawnStatsTexts()) == joined({kCaveStats}));

    cwEntranceModel* entrances = cave->entrances();
    const int entranceRow = entrances->addEntrance(kStation);
    REQUIRE(entranceRow >= 0);

    const QString label = QStringLiteral("North Shaft");
    QSignalSpy changedSpy(f.source, &cwMarkerSource::markersChanged);
    QSignalSpy insertedSpy(f.source, &cwMarkerSource::markersInserted);
    QSignalSpy removedSpy(f.source, &cwMarkerSource::markersRemoved);
    QSignalSpy resetSpy(f.source, &cwMarkerSource::markersReset);
    REQUIRE(entrances->setData(entrances->index(entranceRow), label, cwEntranceModel::LabelRole));

    REQUIRE(waitUntil([&] { return f.drawnTexts() == QStringList{label}; }));
    // One pass changes the one marker in place.
    CHECK(changedSpy.count() == 1);
    CHECK(insertedSpy.count() == 0);
    CHECK(removedSpy.count() == 0);
    CHECK(resetSpy.count() == 0);
    // The label names the entrance; the stats line stays the cave's.
    CHECK(joined(f.drawnStatsTexts()) == joined({kCaveStats}));

    REQUIRE(f.keywords.rowCount() == 1);
    CHECK(keywordPairs(f.keywords.item(0)).contains(QStringLiteral("Name=North Shaft")));

    // Clearing the label falls back to the cave's name, in one more pass.
    REQUIRE(entrances->setData(entrances->index(entranceRow), QString(), cwEntranceModel::LabelRole));
    REQUIRE(waitUntil([&] { return f.drawnTexts() == QStringList{kCaveName}; }));
    CHECK(changedSpy.count() == 2);
}

TEST_CASE("The entrance MarkerView names an entrance on a section by its cave, with the cave's numbers",
          "[cwEntranceMarkerSource]")
{
    EntranceFixture f;
    cwCave* cave = f.addCave(kCaveName);
    const QString sectionName = QStringLiteral("Upper Section");
    cwCave* section = SurveyTreeTestHelper::addNode(&f.region, cave, cwSurveyNode::Kind::Folder,
                                                    sectionName);
    section->fixStations()->appendFixStation(placedFix(kStation));
    f.solve(section, {{kStation, kSolvedPosition}});
    SurveyTreeTestHelper::setSolvedEntrances(section, {solvedEntrance(kStation, kSolvedPosition, true)});
    REQUIRE(section->scopedPositions().position(kStation) == kSolvedPosition);
    // The section's own numbers differ from the cave's, so a stats line drawn
    // from the section fails the check below.
    REQUIRE(section->length()->value() != kCaveLengthMeters);

    REQUIRE(f.waitForOneEntrance());
    CHECK(joined(f.drawnTexts()) == joined({kCaveName}));
    // Line 2 is the cave-wide length and depth.
    CHECK(joined(f.drawnStatsTexts()) == joined({kCaveStats}));

    REQUIRE(f.keywords.rowCount() == 1);
    CHECK(keywordPairs(f.keywords.item(0)).contains(QStringLiteral("Type=Entrance")));
}

TEST_CASE("The entrance MarkerView redraws the stats line when the cave's length changes", "[cwEntranceMarkerSource]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    CHECK(joined(f.drawnStatsTexts()) == joined({kCaveStats}));

    QSignalSpy changedSpy(f.source, &cwMarkerSource::markersChanged);
    cave->length()->setValue(kLongerCaveLengthMeters);

    REQUIRE(waitUntil([&] { return f.drawnStatsTexts() == QStringList{kLongerCaveStats}; }));
    CHECK(joined(f.drawnTexts()) == joined({kCaveName}));
    CHECK(changedSpy.count() == 1);
}

TEST_CASE("The entrance MarkerView draws nothing for a fixed station the solve did not flag as an entrance",
          "[cwEntranceMarkerSource]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();

    // B1 is fixed, placed and solved, and no entrance.
    const QString fixedOnly = QStringLiteral("B1");
    const QVector3D fixedOnlyPosition(-40.0f, 2.0f, 0.0f);
    cave->fixStations()->appendFixStation(placedFix(fixedOnly));
    f.solve(cave, {{kStation, kSolvedPosition}, {fixedOnly, fixedOnlyPosition}});
    REQUIRE(cave->fixStations()->fixStations().at(1).hasPlacedCoordinate());
    REQUIRE(cave->scopedPositions().position(fixedOnly) == fixedOnlyPosition);

    // The solved entrance on A1 is the positive control: the pass ran and drew it.
    REQUIRE(waitUntil([&] { return f.point() != nullptr; }));
    REQUIRE(waitForQueuedEvents());

    const auto instances = f.pins()->buildInstances();
    REQUIRE(instances.size() == 1);
    CHECK(instances.at(0).worldPosition == kSolvedPosition);
    CHECK(f.texts()->buildRenderSlots().size() == 1);
    CHECK(f.statsTexts()->buildRenderSlots().size() == 1);
    CHECK(f.pins()->billboardCount() == 1);
    CHECK(f.view->count() == 1);
    CHECK(f.keywords.rowCount() == 1);
    CHECK(f.point(QStringLiteral("entrancePoint_B1")) == nullptr);
}

TEST_CASE("The entrance MarkerView draws an unfixed entrance at its solved position", "[cwEntranceMarkerSource]")
{
    EntranceFixture f;
    f.addUnfixedEntranceCave();

    REQUIRE(f.waitForOneEntrance());
    const auto instances = f.pins()->buildInstances();
    CHECK(instances.at(0).worldPosition == kSolvedPosition);
    CHECK(joined(f.drawnTexts()) == joined({kCaveName}));
    CHECK(joined(f.drawnStatsTexts()) == joined({kCaveStats}));
    CHECK(f.keywords.rowCount() == 1);

    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);
    CHECK(point->property("position3D").value<QVector3D>() == kSolvedPosition);
    CHECK_FALSE(point->property("fixed").toBool());
}

TEST_CASE("The entrance MarkerView spells a native entrance as its entrance entry does", "[cwEntranceMarkerSource]")
{
    // Cavern reports native stations in lower case.
    EntranceFixture f;
    cwCave* cave = f.addCave(kCaveName);
    REQUIRE(cave->entrances()->addEntrance(kStation) >= 0);
    SurveyTreeTestHelper::setSolvedEntrances(cave, {solvedEntrance(kStation.toLower(), kSolvedPosition, false)});

    REQUIRE(f.waitForOneEntrance());
    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);
    CHECK(point->property("stationName").toString() == kStation);
    CHECK(f.source->row(cave, kStation) == 0);
}

TEST_CASE("The entrance MarkerView names a labeled entrance by its label and an unlabeled one by its cave",
          "[cwEntranceMarkerSource]")
{
    EntranceFixture f;
    const QString label = QStringLiteral("North Shaft");
    const QVector3D otherPosition(-60.0f, 9.0f, 1.0f);

    cwCave* labeledCave = f.addUnfixedEntranceCave(kCaveName, kSolvedPosition);
    cwEntranceModel* entrances = labeledCave->entrances();
    const int entranceRow = entrances->addEntrance(kStation);
    REQUIRE(entranceRow >= 0);
    REQUIRE(entrances->setData(entrances->index(entranceRow), label, cwEntranceModel::LabelRole));

    const QString unlabeledCaveName = QStringLiteral("Calcite Hollow");
    cwCave* unlabeledCave = f.addUnfixedEntranceCave(unlabeledCaveName, otherPosition);
    REQUIRE(unlabeledCave->entrances()->addEntrance(kStation) >= 0);

    REQUIRE(waitUntil([&] { return f.pins()->buildInstances().size() == 2; }));
    QStringList names = f.drawnTexts();
    names.sort();
    CHECK(joined(names) == joined({unlabeledCaveName, label}));
    // A label replaces the cave's name on line 1 only; both stats lines stay.
    CHECK(f.drawnStatsTexts().size() == 2);
}

TEST_CASE("The entrance MarkerView names an attached entrance by its cave", "[cwEntranceMarkerSource]")
{
    // An attached file's entrance has no survey-editor row, so its owning node
    // (the source root under the cave) holds no entrance entry and no fix.
    EntranceFixture f;
    cwCave* cave = f.addCave(kCaveName);
    cwCave* sourceRoot = SurveyTreeTestHelper::addNode(&f.region, cave, cwSurveyNode::Kind::Folder,
                                                       QStringLiteral("doghill.svx"));
    const QString attachedStation = QStringLiteral("doghill.d3");
    SurveyTreeTestHelper::setSolvedEntrances(sourceRoot,
                                             {solvedEntrance(attachedStation, kSolvedPosition, false)});
    REQUIRE(sourceRoot->entrances()->rowCount() == 0);
    REQUIRE(sourceRoot->fixStations()->rowCount() == 0);

    REQUIRE(f.waitForOneEntrance());
    CHECK(joined(f.drawnTexts()) == joined({kCaveName}));
    CHECK(joined(f.drawnStatsTexts()) == joined({kCaveStats}));

    QQuickItem* point = f.point(QStringLiteral("entrancePoint_") + attachedStation);
    REQUIRE(point != nullptr);
    CHECK(point->property("caveName").toString() == kCaveName);
    CHECK(point->property("label").toString().isEmpty());
    CHECK(point->property("node").value<cwSurveyNode*>() == sourceRoot);
    CHECK(point->property("caveNode").value<cwSurveyNode*>() == cave);
}

TEST_CASE("The entrance MarkerView renames an entrance's line 1 from its label without a re-solve",
          "[cwEntranceMarkerSource]")
{
    EntranceFixture f;
    cwCave* cave = f.addUnfixedEntranceCave();
    cwEntranceModel* entrances = cave->entrances();
    const int entranceRow = entrances->addEntrance(kStation);
    REQUIRE(entranceRow >= 0);
    REQUIRE(f.waitForOneEntrance());
    REQUIRE(joined(f.drawnTexts()) == joined({kCaveName}));

    QSignalSpy solvedSpy(&f.region, &cwCavingRegion::solvedEntrancesChanged);
    QSignalSpy changedSpy(f.source, &cwMarkerSource::markersChanged);
    QSignalSpy insertedSpy(f.source, &cwMarkerSource::markersInserted);
    QSignalSpy removedSpy(f.source, &cwMarkerSource::markersRemoved);

    const QString label = QStringLiteral("Sinkhole Entrance");
    REQUIRE(entrances->setData(entrances->index(entranceRow), label, cwEntranceModel::LabelRole));
    REQUIRE(waitUntil([&] { return f.drawnTexts() == QStringList{label}; }));
    REQUIRE(waitForQueuedEvents());

    CHECK(changedSpy.count() == 1);
    CHECK(insertedSpy.count() == 0);
    CHECK(removedSpy.count() == 0);
    CHECK(solvedSpy.count() == 0);
    CHECK(f.point()->property("label").toString() == label);
    CHECK(f.point()->property("displayName").toString() == label);
}

TEST_CASE("The entrance pin carries the cave glyph and the per-kind pins are gone", "[cwEntranceMarkerSource]")
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

TEST_CASE("The entrance MarkerView drops the pin, text and keyword item of an entrance the next solve drops",
          "[cwEntranceMarkerSource]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    REQUIRE(f.keywords.rowCount() == 1);
    CHECK(f.statsTexts()->billboardCount() == 1);

    // The fix stays placed and solved; only the solve's entrance flag goes.
    QSignalSpy removedSpy(f.source, &cwMarkerSource::markersRemoved);
    SurveyTreeTestHelper::setSolvedEntrances(cave, {});
    REQUIRE(cave->fixStations()->fixStations().at(0).hasPlacedCoordinate());

    REQUIRE(waitUntil([&] {
        return f.pins()->billboardCount() == 0
               && f.texts()->billboardCount() == 0
               && f.statsTexts()->billboardCount() == 0
               && f.keywords.rowCount() == 0
               && f.point() == nullptr;
    }));
    CHECK(f.view->count() == 0);
    CHECK(removedSpy.count() == 1);
}

TEST_CASE("The entrance MarkerView's keyword item names the fix and hides it", "[cwEntranceMarkerSource]")
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
    CHECK(f.view->count() == 1);
    CHECK(f.keywords.rowCount() == 1);

    proxy->setVisible(true);
    REQUIRE(f.waitForOneEntrance());
    CHECK(point->isVisible());
    CHECK(joined(f.drawnTexts()) == joined({kCaveName}));
    CHECK(joined(f.drawnStatsTexts()) == joined({kCaveStats}));
}

TEST_CASE("The entrance MarkerView styles the name and stats layers from its properties", "[cwEntranceMarkerSource]")
{
    EntranceFixture f;
    f.view->setTextGap(kTextGap);

    QFont nameFont;
    nameFont.setPixelSize(kNameFontPixels);
    f.nameStyle->setFont(nameFont);
    f.nameStyle->setColor(kNameColor);
    f.nameStyle->setOutlineColor(kNameOutlineColor);

    QFont statsFont;
    statsFont.setPixelSize(kStatsFontPixels);
    QSignalSpy statsFontSpy(f.statsStyle, &cwMarkerTextStyle::fontChanged);
    QSignalSpy statsColorSpy(f.statsStyle, &cwMarkerTextStyle::colorChanged);
    QSignalSpy statsOutlineColorSpy(f.statsStyle, &cwMarkerTextStyle::outlineColorChanged);
    f.statsStyle->setFont(statsFont);
    f.statsStyle->setColor(kStatsColor);
    f.statsStyle->setOutlineColor(kStatsOutlineColor);
    CHECK(statsFontSpy.count() == 1);
    CHECK(statsColorSpy.count() == 1);
    CHECK(statsOutlineColorSpy.count() == 1);
    CHECK(f.statsStyle->font().pixelSize() == kStatsFontPixels);
    CHECK(f.statsStyle->color() == kStatsColor);
    CHECK(f.statsStyle->outlineColor() == kStatsOutlineColor);

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
        f.statsStyle->setColor(kOtherStatsColor);
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
        f.statsStyle->setFont(largerStatsFont);
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
    int markersInserted = 0;
    // Every other ranged signal: reset, removed and changed.
    int otherMarkerSignals = 0;
};

// Adds @a nodeCount caves with one placed, solved fix each in one event-loop
// turn, then counts what the view emitted for that burst.
BurstCounts addEntranceBurst(int nodeCount)
{
    EntranceFixture f;
    REQUIRE(waitForQueuedEvents());

    QSignalSpy rowsInsertedSpy(&f.keywords, &QAbstractItemModel::rowsInserted);
    QSignalSpy insertedSpy(f.source, &cwMarkerSource::markersInserted);
    QSignalSpy resetSpy(f.source, &cwMarkerSource::markersReset);
    QSignalSpy removedSpy(f.source, &cwMarkerSource::markersRemoved);
    QSignalSpy changedSpy(f.source, &cwMarkerSource::markersChanged);

    for (int i = 0; i < nodeCount; ++i) {
        f.addEntranceCave(QStringLiteral("Cave %1").arg(i + 1));
    }

    INFO("nodes " << nodeCount);
    REQUIRE(waitUntil([&] {
        return f.view->count() == nodeCount && f.keywords.rowCount() == nodeCount;
    }));
    REQUIRE(waitForQueuedEvents());
    CHECK(f.pins()->buildInstances().size() == nodeCount);

    return {int(rowsInsertedSpy.count()), int(insertedSpy.count()),
            int(resetSpy.count() + removedSpy.count() + changedSpy.count())};
}

} // namespace

TEST_CASE("The entrance MarkerView rebuilds once per burst and grows its keyword inserts linearly",
          "[cwEntranceMarkerSource]")
{
    const BurstCounts small = addEntranceBurst(kSmallRegionNodes);
    const BurstCounts large = addEntranceBurst(kSmallRegionNodes * kGrowthFactor);

    INFO("keyword rowsInserted small " << small.keywordRowsInserted
         << " large " << large.keywordRowsInserted);
    CHECK(small.keywordRowsInserted >= 1);
    CHECK(large.keywordRowsInserted <= kGrowthFactor * small.keywordRowsInserted);

    // One queued pass appends the burst as one inserted range.
    CHECK(small.markersInserted == 1);
    CHECK(large.markersInserted == 1);
    CHECK(small.otherMarkerSignals == 0);
    CHECK(large.otherMarkerSignals == 0);
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
// down -Z) than the solved station, centered near @a tap's ray. @a f is any
// fixture with a camera and a scene.
template <typename Fixture>
void addWallInFront(Fixture& f, const QPointF& tap)
{
    const QRay3D ray = f.camera.rayFromQtViewport(tap);
    const QVector3D center = ray.point(ray.origin().z() - kSolvedPosition.z()) + kWallCenterOffset
                             + QVector3D(0.0f, 0.0f, kWallInFront);
    f.scene.geometryItersecter()->addObject(cwGeometryItersecter::Object(
        nullptr, kWallId, cwTestGeometry::zPlaneQuad(center, kWallHalfExtent)));
    f.scene.geometryItersecter()->waitForFinish();
}

} // namespace

TEST_CASE("The entrance MarkerView tests a tap against its tip-down pin", "[cwEntranceMarkerSource]")
{
    EntranceFixture f;
    f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    REQUIRE(f.view->spriteSize() == kPinSize);

    const QPointF station = f.camera.project(kSolvedPosition);
    CHECK_FALSE(f.view->isOccluded(kSolvedPosition, station + kOnPinBody));
    CHECK(f.view->isOccluded(kSolvedPosition, station + kBelowTip));
}

TEST_CASE("The entrance MarkerView reports a pin behind a wall as occluded", "[cwEntranceMarkerSource]")
{
    EntranceFixture f;
    f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());

    const QPointF tap = f.camera.project(kSolvedPosition) + kOnPinBody;
    REQUIRE_FALSE(f.view->isOccluded(kSolvedPosition, tap));

    addWallInFront(f, tap);
    CHECK(f.view->isOccluded(kSolvedPosition, tap));
}

TEST_CASE("The entrance MarkerView takes a tap on a pin behind a wall while the pin draws",
          "[cwEntranceMarkerSource]")
{
    auto* settings = cwRenderingSettings::instance();
    REQUIRE(settings != nullptr);
    cwMarkerKnobsReset knobsReset;

    EntranceFixture f;
    f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());

    const QPointF station = f.camera.project(kSolvedPosition);
    const QPointF onPin = station + kOnPinBody;
    const QPointF offPin = station + kBelowTip;
    addWallInFront(f, onPin);
    REQUIRE(f.view->isOccluded(kSolvedPosition, onPin));

    // The defaults draw the hidden pin as an x-ray ghost, which takes the tap.
    CHECK(f.view->isTappable(kSolvedPosition, onPin));
    CHECK_FALSE(f.view->isTappable(kSolvedPosition, offPin));

    // The depth test on with x-ray off hides the pin, and the wall keeps the tap.
    settings->setMarkerXRay(false);
    CHECK_FALSE(f.view->isTappable(kSolvedPosition, onPin));
    CHECK_FALSE(f.view->isTappable(kSolvedPosition, offPin));

    // The depth test off draws the pin over the wall; a tap off the pin still misses.
    settings->setMarkerDepthTest(false);
    CHECK(f.view->isTappable(kSolvedPosition, onPin));
    CHECK_FALSE(f.view->isTappable(kSolvedPosition, offPin));
}

namespace {

// The marker knobs' ghost opacities as 0-1 factors and readout percents.
constexpr float kBaseGhostOpacity = 0.35f;
constexpr float kOverheadGhostOpacity = 1.0f;
constexpr float kMidRampGhostOpacity = 0.675f;
constexpr float kGhostOpacityTolerance = 1e-4f;
constexpr double kPercentScale = 100.0;

// Camera tilts below the horizon, in degrees.
constexpr double kStraightDownTilt = 90.0;
constexpr double kLevelTilt = 0.0;
constexpr double kMidRampTilt = 72.5;
constexpr double kHalfwayDownTilt = 45.0;

// A view matrix tilted @a tiltDegrees below the horizon, the turntable's pitch:
// the identity looks straight down.
QMatrix4x4 tiltedView(double tiltDegrees)
{
    QMatrix4x4 view;
    view.rotate(float(tiltDegrees - kStraightDownTilt), 1.0f, 0.0f, 0.0f);
    return view;
}

// @a view moved without changing its tilt: turned about the vertical and panned.
QMatrix4x4 turnedAndPanned(const QMatrix4x4& view)
{
    constexpr float kAzimuthDegrees = 30.0f;
    const QVector3D kPan(12.0f, -7.0f, -50.0f);
    QMatrix4x4 moved = view;
    moved.rotate(kAzimuthDegrees, 0.0f, 0.0f, 1.0f);
    moved.translate(kPan);
    return moved;
}

// Every entrance layer draws its ghost at @a opacity, and the view reads it out.
void checkGhostOpacity(EntranceFixture& f, bool xRay, float opacity)
{
    const QList<QPair<const char*, cwBillboardDepthMode>> layers = {
        {"pins", f.pins()->depthMode()},
        {"names", f.texts()->depthMode()},
        {"stats", f.statsTexts()->depthMode()},
    };
    for (const auto& layer : layers) {
        INFO(layer.first);
        CHECK(layer.second.xRay == xRay);
        CHECK_THAT(layer.second.xRayOpacity, Catch::Matchers::WithinAbs(opacity, kGhostOpacityTolerance));
    }
    CHECK_THAT(f.view->xRayOpacityPercent(),
               Catch::Matchers::WithinAbs(opacity * kPercentScale, kGhostOpacityTolerance * kPercentScale));
}

} // namespace

TEST_CASE("The entrance MarkerView ramps its x-ray ghost with the camera tilt and reads the opacity out",
          "[cwEntranceMarkerSource]")
{
    auto* settings = cwRenderingSettings::instance();
    REQUIRE(settings != nullptr);
    cwMarkerKnobsReset knobsReset;

    EntranceFixture f;
    f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    REQUIRE(f.camera.viewMatrix() == QMatrix4x4());

    // The fixture's camera looks straight down, where the ghost is at the
    // overhead knob.
    checkGhostOpacity(f, true, kOverheadGhostOpacity);
    QSignalSpy readoutSpy(f.view, &cwMarkerView::xRayOpacityPercentChanged);

    SECTION("orbiting from straight down to level fades the ghost to the base knob and back") {
        f.camera.setViewMatrix(tiltedView(kLevelTilt));
        checkGhostOpacity(f, true, kBaseGhostOpacity);
        CHECK(readoutSpy.count() == 1);

        f.camera.setViewMatrix(tiltedView(kMidRampTilt));
        checkGhostOpacity(f, true, kMidRampGhostOpacity);
        CHECK(readoutSpy.count() == 2);

        f.camera.setViewMatrix(QMatrix4x4());
        checkGhostOpacity(f, true, kOverheadGhostOpacity);
        CHECK(readoutSpy.count() == 3);

        // Turning and panning keep the tilt, so the opacity and the readout stay.
        f.camera.setViewMatrix(turnedAndPanned(QMatrix4x4()));
        checkGhostOpacity(f, true, kOverheadGhostOpacity);
        CHECK(readoutSpy.count() == 3);

        f.camera.setViewMatrix(tiltedView(kLevelTilt));
        CHECK(readoutSpy.count() == 4);
        f.camera.setViewMatrix(turnedAndPanned(tiltedView(kLevelTilt)));
        checkGhostOpacity(f, true, kBaseGhostOpacity);
        CHECK(readoutSpy.count() == 4);
    }

    SECTION("each ramp knob reaches the view at once") {
        constexpr int kHalfOverheadPercent = 50;
        constexpr float kHalfOverheadOpacity = 0.5f;
        constexpr int kEarlyRampStart = 5;
        // Halfway from 35 to 50 percent, at 45 degrees on a 5-85 ramp.
        constexpr float kHalfRampOpacity = 0.425f;

        settings->setMarkerXRayOverheadOpacityPercent(kHalfOverheadPercent);
        checkGhostOpacity(f, true, kHalfOverheadOpacity);
        CHECK(readoutSpy.count() == 1);

        f.camera.setViewMatrix(tiltedView(kHalfwayDownTilt));
        checkGhostOpacity(f, true, kBaseGhostOpacity);
        CHECK(readoutSpy.count() == 2);

        settings->setMarkerXRayRampStartTiltDegrees(kEarlyRampStart);
        checkGhostOpacity(f, true, kHalfRampOpacity);
        CHECK(readoutSpy.count() == 3);

        settings->setMarkerXRayRampEndTiltDegrees(int(kHalfwayDownTilt));
        checkGhostOpacity(f, true, kHalfOverheadOpacity);
        CHECK(readoutSpy.count() == 4);
    }

    SECTION("x-ray off and the depth test off skip the ramp") {
        settings->setMarkerXRay(false);
        checkGhostOpacity(f, false, kBaseGhostOpacity);
        CHECK(readoutSpy.count() == 1);

        f.camera.setViewMatrix(tiltedView(kMidRampTilt));
        checkGhostOpacity(f, false, kBaseGhostOpacity);
        CHECK(readoutSpy.count() == 1);

        // Level, x-ray on lands on the same base opacity, so the readout stays.
        f.camera.setViewMatrix(tiltedView(kLevelTilt));
        settings->setMarkerXRay(true);
        checkGhostOpacity(f, true, kBaseGhostOpacity);
        CHECK(readoutSpy.count() == 1);

        settings->setMarkerDepthTest(false);
        f.camera.setViewMatrix(QMatrix4x4());
        CHECK_FALSE(f.pins()->depthMode().depthTest);
        CHECK_THAT(f.view->xRayOpacityPercent(),
                   Catch::Matchers::WithinAbs(kBaseGhostOpacity * kPercentScale,
                                              kGhostOpacityTolerance * kPercentScale));
        CHECK(readoutSpy.count() == 1);
    }
}

TEST_CASE("The entrance MarkerView selects an entrance's point by its source row", "[cwEntranceMarkerSource]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    cwCave* otherCave = f.addCave(QStringLiteral("Other Cave"));
    REQUIRE(f.waitForOneEntrance());
    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);

    const int row = f.source->row(cave, kStation);
    REQUIRE(row == 0);
    f.view->select(row);
    CHECK(f.view->selectionManager()->selectedItem() == point);
    CHECK(point->property("selected").toBool());

    // Station names match as the entrance list matches them.
    CHECK(f.source->row(cave, kStation.toLower()) == 0);
    // The entrance belongs to its own node only.
    CHECK(f.source->row(otherCave, kStation) == -1);
    CHECK(f.source->row(cave, QStringLiteral("B1")) == -1);
}

TEST_CASE("The entrance MarkerView takes a settable selection manager and hands it to its points",
          "[cwEntranceMarkerSource]")
{
    cwSelectionManager otherSelection;
    EntranceFixture f;
    f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);

    QSignalSpy changedSpy(f.view, &cwMarkerView::selectionManagerChanged);
    f.view->setSelectionManager(&otherSelection);
    CHECK(changedSpy.count() == 1);
    CHECK(f.view->selectionManager() == &otherSelection);
    CHECK(point->property("selectionManager").value<cwSelectionManager*>() == &otherSelection);

    f.view->setSelectionManager(&otherSelection);
    CHECK(changedSpy.count() == 1);
}

TEST_CASE("The entrance MarkerView selects nothing without a selection manager", "[cwEntranceMarkerSource]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);
    const int row = f.source->row(cave, kStation);
    REQUIRE(row == 0);
    f.view->setSelectionManager(nullptr);
    CHECK(f.view->selectionManager() == nullptr);

    f.view->select(row);
    CHECK_FALSE(point->property("selected").toBool());
}

TEST_CASE("The entrance MarkerView clears its selected point from a manager it replaces", "[cwEntranceMarkerSource]")
{
    cwSelectionManager otherSelection;
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);
    const int row = f.source->row(cave, kStation);
    REQUIRE(row == 0);
    f.view->select(row);
    CHECK(f.selection.selectedItem() == point);

    f.view->setSelectionManager(&otherSelection);
    CHECK(f.selection.selectedItem() == nullptr);
    CHECK_FALSE(point->property("selected").toBool());
    CHECK(otherSelection.selectedItem() == nullptr);
}

TEST_CASE("The entrance MarkerView hands each point its cave, label, station and fixed flag and no kind",
          "[cwEntranceMarkerSource]")
{
    EntranceFixture f;
    cwCave* cave = f.addEntranceCave();
    REQUIRE(f.waitForOneEntrance());
    QQuickItem* point = f.point();
    REQUIRE(point != nullptr);

    CHECK(point->property("displayName").toString() == kCaveName);
    CHECK(point->property("caveName").toString() == kCaveName);
    CHECK(point->property("label").toString().isEmpty());
    CHECK(point->property("stationName").toString() == kStation);
    CHECK(point->property("fixed").toBool());
    CHECK(point->property("node").value<cwSurveyNode*>() == cave);
    CHECK(point->property("caveNode").value<cwSurveyNode*>() == cave);
    // Kind is gone from the point and its popup.
    CHECK_FALSE(point->property("kind").isValid());
    CHECK_FALSE(point->property("kindName").isValid());

    cwEntranceModel* entrances = cave->entrances();
    const int entranceRow = entrances->addEntrance(kStation);
    REQUIRE(entranceRow >= 0);
    const QString label = QStringLiteral("North Shaft");
    REQUIRE(entrances->setData(entrances->index(entranceRow), label, cwEntranceModel::LabelRole));

    CHECK(waitUntil([&] { return point->property("displayName").toString() == label; }));
    CHECK(point->property("label").toString() == label);
    CHECK(point->property("caveName").toString() == kCaveName);
}

TEST_CASE("The entrance MarkerView draws a native unfixed entrance the solve flags",
          "[cwEntranceMarkerSource]")
{
    // A native survey with no fix: cavern solves it from the origin the driver
    // invents and flags the marked entrance a3, which the solve reports unfixed.
    // A cavern run on a loaded machine can take a while.
    constexpr int kSolveWaitMs = 20000;
    const QString entranceStation = QStringLiteral("a3");

    EntranceFixture f;
    cwCave* cave = f.addCave(kCaveName);
    cwTrip* trip = SurveyTreeTestHelper::addTrip(cave, QStringLiteral("Chain"), QStringLiteral("a"));
    SurveyTreeTestHelper::addShot(trip, QStringLiteral("a2"), entranceStation);
    REQUIRE(cave->entrances()->addEntrance(entranceStation) >= 0);
    REQUIRE(cave->fixStations()->rowCount() == 0);

    cwLinePlotManager manager;
    manager.setRegion(&f.region);
    manager.waitToFinish();
    INFO("driver:\n" << manager.driverSource().toStdString());
    INFO("cavern log:\n" << manager.cavernLog().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());
    REQUIRE(manager.isSolvedEntrance(cave->id(), entranceStation));

    QQuickItem* point = nullptr;
    REQUIRE(waitUntil([&] {
        point = f.point(QStringLiteral("entrancePoint_") + entranceStation);
        return point != nullptr;
    }, kSolveWaitMs));
    CHECK_FALSE(point->property("fixed").toBool());
    CHECK(point->property("caveName").toString() == kCaveName);
    REQUIRE(cave->scopedPositions().hasPosition(entranceStation));
    CHECK(point->property("position3D").value<QVector3D>()
          == cave->scopedPositions().position(entranceStation));
    CHECK(f.view->count() == 1);
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
               "  MarkerView {\n"
               "    declutterTier: LabelDeclutter.EntranceTier\n"
               "%2"
               "  }\n"
               "}\n")
        .arg(kViewportSize)
        .arg(entranceMarkerViewBody());
}

QRectF centeredRect(const QPointF& center, const QSizeF& size)
{
    return QRectF(center - QPointF(size.width() / 2.0, size.height() / 2.0), size);
}

// One entrance MarkerView and two Label3dViews in one scene: "stationLabelView" joins
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
    cwMarkerView* view = nullptr;
    cwEntranceMarkerSource* source = nullptr;
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

        view = root->findChild<cwMarkerView*>(QStringLiteral("entranceView"));
        labelView = root->findChild<cwLabel3dView*>(QStringLiteral("stationLabelView"));
        privateLabelView = root->findChild<cwLabel3dView*>(QStringLiteral("privateLabelView"));
        REQUIRE(view != nullptr);
        REQUIRE(labelView != nullptr);
        REQUIRE(privateLabelView != nullptr);
        REQUIRE(view->declutterTier() == cwLabelDeclutter::EntranceTier);
        REQUIRE(labelView->declutterTier() == cwLabelDeclutter::StationLabelTier);
        REQUIRE(privateLabelView->declutterTier() == cwLabelDeclutter::NoTier);
        source = qobject_cast<cwEntranceMarkerSource*>(view->source());
        REQUIRE(source != nullptr);
        cwMarkerTextStyle* nameStyle = textStyle(view, 0);
        cwMarkerTextStyle* statsStyle = textStyle(view, 1);
        REQUIRE(nameStyle != nullptr);
        REQUIRE(statsStyle != nullptr);

        QFont nameFont;
        nameFont.setPixelSize(kNameFontPixels);
        nameStyle->setFont(nameFont);
        QFont statsFont;
        statsFont.setPixelSize(kStatsFontPixels);
        statsStyle->setFont(statsFont);
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
        source->setRegion(&region);
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
        SurveyTreeTestHelper::setSolvedEntrances(cave, {solvedEntrance(kStation, position, true)});
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
    cwRenderTextBillboards* statsTexts() { return scene.textBillboardLayer(kStatsLayerKey); }
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

TEST_CASE("An entrance behind a wall claims its pin and text space only while it draws",
          "[cwLabelDeclutter]")
{
    auto* settings = cwRenderingSettings::instance();
    REQUIRE(settings != nullptr);
    cwMarkerKnobsReset knobsReset;

    DeclutterFixture f;
    const QVector3D farPosition = f.worldAtPixelOffset(kSolvedPosition, QPointF(0.0, kFarPixels));
    DeclutterFixture::addLabels(f.labelView, {cwLabel3dItem(kLabelOnPin, kSolvedPosition),
                                              cwLabel3dItem(kLabelFar, farPosition)});
    REQUIRE(waitUntil([&] { return f.isStationLabelDrawn(kLabelOnPin) && f.isStationLabelDrawn(kLabelFar); }));

    addWallInFront(f, f.camera.project(kSolvedPosition) + kOnPinBody);
    f.addEntranceCaveAt(kCaveName, kSolvedPosition);
    REQUIRE(waitUntil([&] { return f.pinCount() == 1; }));
    REQUIRE(f.labelTextRect(kSolvedPosition, kLabelOnPin).intersects(f.pinRect(kSolvedPosition)));

    // The defaults draw the hidden entrance as an x-ray ghost: it keeps its
    // text and its pin pushes the station label out.
    INFO("station labels: " << joined(f.drawnStationLabels()) << " names: " << joined(f.drawnNames()));
    CHECK(waitUntil([&] { return !f.isStationLabelDrawn(kLabelOnPin)
                                 && f.drawnNames() == QStringList{kCaveName}; }));

    // The depth test on with x-ray off hides the entrance, so it claims nothing:
    // its text goes, and the station label over its pin comes back.
    settings->setMarkerXRay(false);
    CHECK(waitUntil([&] { return f.isStationLabelDrawn(kLabelOnPin) && f.drawnNames().isEmpty(); }));
    CHECK(f.drawnStats().isEmpty());
    CHECK(f.pinCount() == 1);
    CHECK(f.isStationLabelDrawn(kLabelFar));

    // A camera change runs the pass again with the same outcome.
    f.pan();
    CHECK(f.isStationLabelDrawn(kLabelOnPin));
    CHECK(f.drawnNames().isEmpty());

    // The depth test off draws the entrance over the wall, so it claims again.
    settings->setMarkerDepthTest(false);
    CHECK(waitUntil([&] { return !f.isStationLabelDrawn(kLabelOnPin)
                                 && f.drawnNames() == QStringList{kCaveName}; }));
    CHECK(joined(f.drawnStats()) == joined({kCaveStats}));
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
