// Pins cwMarkerView, the one view leads and entrances draw through, against a
// fake cwMarkerSource: each ranged source signal (reset, inserted, removed,
// changed) leaves one delegate per marker at the marker's position, one sprite
// instance per marker in the view's keyed layer, and each text line in the
// layer of its style. Also pins cwMarkerSource's own contract, and that a 4x
// larger source costs at most about 4x in signals and layer pushes. The marker
// knobs in cwRenderingSettings set the depth mode of every layer the view
// draws, and leave the station labels' default text layer alone. The ghost's
// opacity ramps with the camera tilt, so the base-opacity cases look level.
//
// The view is built in QML because its delegates come from a QML component.
// The window is never shown and there is no QRhi: every check reads the CPU
// side of the scene's keyed layers.

//Catch includes
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTextLayout>

// Std
#include <algorithm>
#include <memory>

// SUT
#include "cwCamera.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwMarkerSource.h"
#include "cwMarkerView.h"
#include "cwPositionItem.h"
#include "cwMarkerKnobsReset.h"
#include "cwRenderingSettings.h"
#include "cwRenderSpriteBillboards.h"
#include "cwRenderTextBillboards.h"
#include "cwRhiScene.h"
#include "cwScene.h"
#include "cwSelectionManager.h"
#include "cwVisibilityProxy.h"
#include "CwRhiSceneTestAccess.h"
#include "TestHelper.h"

namespace {

// Exposes the protected mutators, so a test edits the marker list directly.
class FakeMarkerSource : public cwMarkerSource
{
public:
    using cwMarkerSource::cwMarkerSource;
    using cwMarkerSource::resetMarkers;
    using cwMarkerSource::insertMarkers;
    using cwMarkerSource::removeMarkers;
    using cwMarkerSource::replaceMarkers;
};

constexpr int kViewportSize = 400;
const QSizeF kDotSize(10.0, 10.0);

const QString kLayerKey = QStringLiteral("testMarkers");
const QString kDetailLayerKey = QStringLiteral("testMarkerDetails");
const QString kTitleStyle = QStringLiteral("title");
const QString kDetailStyle = QStringLiteral("detail");
const QString kMissingStyle = QStringLiteral("missing");
const QString kTagProperty = QStringLiteral("tag");
const QString kDelegatePrefix = QStringLiteral("testMarker_");
const QString kMarkerType = QStringLiteral("TestMarker");

// Logical pixels between a sprite and the text above it.
constexpr qreal kTextGap = 3.0;
// World units between neighboring test markers along x, y and z.
const QVector3D kMarkerStep(3.0f, -1.0f, 0.5f);
// Where a changed marker moves to, clear of every markerPosition().
const QVector3D kMovedPosition(-30.0f, 12.0f, 4.0f);
// A row past the end of every list here.
constexpr int kOutOfRangeRow = 5;

// A 4x larger source may cost at most this many times the signals and pushes.
constexpr int kGrowthFactor = 4;
constexpr int kSmallMarkerCount = 8;

// The world position of marker @a index, distinct per index.
QVector3D markerPosition(int index)
{
    return kMarkerStep * float(index);
}

// Marker @a index: a title line over a detail line, tagged "m<index>", alone
// in its own visibility group unless @a group names one.
cwMarker testMarker(int index, const QString& group = QString())
{
    cwMarker marker;
    marker.id = QUuid::createUuid();
    marker.position = markerPosition(index);
    marker.textLines = {{QStringLiteral("Title %1").arg(index), kTitleStyle},
                        {QStringLiteral("Detail %1").arg(index), kDetailStyle}};
    marker.visibilityGroup = group.isEmpty() ? QStringLiteral("group%1").arg(index) : group;
    marker.keywords = {cwKeyword(cwKeywordModel::TypeKey, kMarkerType),
                       cwKeyword(cwKeywordModel::NameKey, marker.visibilityGroup)};
    marker.delegateProperties = {{kTagProperty, QStringLiteral("m%1").arg(index)}};
    return marker;
}

QList<cwMarker> testMarkers(int first, int count)
{
    QList<cwMarker> markers;
    for (int i = first; i < first + count; ++i) {
        markers.append(testMarker(i));
    }
    return markers;
}

QString markerViewQml()
{
    return QStringLiteral(
               "import QtQuick\n"
               "import cavewherelib\n"
               "Item {\n"
               "  width: %1; height: %1\n"
               "  MarkerView {\n"
               "    objectName: \"markerView\"\n"
               "    anchors.fill: parent\n"
               "    layerKey: \"%2\"\n"
               "    sprites: [dot]\n"
               "    delegate: PositionItem {\n"
               "      objectName: \"%3\" + tag\n"
               "      property string tag\n"
               "      property bool selected: false\n"
               "      property MarkerView markerView\n"
               "      property SelectionManager selectionManager\n"
               "      property alias marker: hitTarget\n"
               "      Item { id: hitTarget; width: %4; height: %5; anchors.centerIn: parent }\n"
               "    }\n"
               "    textStyles: [\n"
               "      MarkerTextStyle { styleKey: \"%6\" },\n"
               "      MarkerTextStyle { styleKey: \"%7\"; layerKey: \"%8\" }\n"
               "    ]\n"
               "    Item { id: dot; width: %4; height: %5 }\n"
               "  }\n"
               "}\n")
        .arg(kViewportSize)
        .arg(kLayerKey)
        .arg(kDelegatePrefix)
        .arg(kDotSize.width())
        .arg(kDotSize.height())
        .arg(kTitleStyle)
        .arg(kDetailStyle)
        .arg(kDetailLayerKey);
}

bool lessThan(const QVector3D& a, const QVector3D& b)
{
    if (a.x() != b.x()) {
        return a.x() < b.x();
    }
    if (a.y() != b.y()) {
        return a.y() < b.y();
    }
    return a.z() < b.z();
}

QList<QVector3D> sorted(QList<QVector3D> positions)
{
    std::sort(positions.begin(), positions.end(), lessThan);
    return positions;
}

std::string toString(const QList<QVector3D>& positions)
{
    QStringList parts;
    for (const QVector3D& position : positions) {
        parts.append(QStringLiteral("(%1,%2,%3)").arg(position.x()).arg(position.y()).arg(position.z()));
    }
    return parts.join(QStringLiteral(" ")).toStdString();
}

// A QML MarkerView over a FakeMarkerSource in a window that is never shown.
struct MarkerFixture
{
    FakeMarkerSource source;
    cwKeywordItemModel keywords;
    cwScene scene;
    cwCamera camera;
    cwSelectionManager selection;
    QQmlEngine engine;
    QQuickWindow window;
    std::unique_ptr<QObject> root;
    cwMarkerView* view = nullptr;

    MarkerFixture()
    {
        camera.setViewport(QRect(0, 0, kViewportSize, kViewportSize));
        camera.setProjection(camera.orthoProjectionDefault());
        window.resize(kViewportSize, kViewportSize);

        QQmlComponent component(&engine);
        component.setData(markerViewQml().toUtf8(), QUrl());
        root.reset(component.create());
        INFO(component.errorString().toStdString());
        REQUIRE(root != nullptr);
        auto* rootItem = qobject_cast<QQuickItem*>(root.get());
        REQUIRE(rootItem != nullptr);
        rootItem->setParentItem(window.contentItem());

        view = root->findChild<cwMarkerView*>(QStringLiteral("markerView"));
        REQUIRE(view != nullptr);
        REQUIRE(view->window() == &window);
        REQUIRE(view->layerKey() == kLayerKey);

        view->setScene(&scene);
        view->setCamera(&camera);
        view->setKeywordItemModel(&keywords);
        view->setSelectionManager(&selection);
        view->setSource(&source);
    }

    ~MarkerFixture()
    {
        // The view goes before the source and the models it points at.
        root.reset();
    }

    cwRenderSpriteBillboards* sprites() { return scene.spriteBillboardLayer(kLayerKey); }
    cwRenderTextBillboards* titles() { return scene.textBillboardLayer(kLayerKey); }
    cwRenderTextBillboards* details() { return scene.textBillboardLayer(kDetailLayerKey); }

    QList<QVector3D> spritePositions()
    {
        QList<QVector3D> positions;
        const auto instances = sprites()->buildInstances();
        for (const auto& instance : instances) {
            positions.append(instance.worldPosition);
        }
        return sorted(positions);
    }

    static QStringList drawnTexts(const cwRenderTextBillboards* layer)
    {
        QStringList result;
        const auto renderSlots = layer->buildRenderSlots();
        for (const auto& slot : renderSlots) {
            result.append(slot.layout != nullptr ? slot.layout->text() : QString());
        }
        result.sort();
        return result;
    }

    // The live delegates, ones awaiting deletion included.
    QList<cwPositionItem*> delegates()
    {
        QList<cwPositionItem*> result;
        const auto children = view->findChildren<cwPositionItem*>();
        for (cwPositionItem* child : children) {
            if (child->objectName().startsWith(kDelegatePrefix)) {
                result.append(child);
            }
        }
        return result;
    }

    cwPositionItem* delegate(const QString& tag)
    {
        return view->findChild<cwPositionItem*>(kDelegatePrefix + tag);
    }

    // Waits until exactly @a count delegates are left, at these positions.
    bool waitForDelegates(const QList<QVector3D>& positions)
    {
        return waitUntil([&] {
            QList<QVector3D> delegatePositions;
            const auto items = delegates();
            for (cwPositionItem* item : items) {
                delegatePositions.append(item->position3D());
            }
            return sorted(delegatePositions) == sorted(positions);
        });
    }
};

QList<QVector3D> positionsOf(const QList<cwMarker>& markers)
{
    QList<QVector3D> positions;
    for (const cwMarker& marker : markers) {
        positions.append(marker.position);
    }
    return sorted(positions);
}

QStringList sortedList(QStringList list)
{
    list.sort();
    return list;
}

// The visibility target of the keyword item named @a name, or null.
cwVisibilityProxy* proxyNamed(const cwKeywordItemModel& keywords, const QString& name)
{
    for (int row = 0; row < keywords.rowCount(); ++row) {
        cwKeywordItem* item = keywords.item(row);
        const cwKeywordModel* model = item->keywordModel();
        for (int keyRow = 0; keyRow < model->rowCount(); ++keyRow) {
            const QModelIndex index = model->index(keyRow);
            if (model->data(index, cwKeywordModel::KeyRole).toString() == cwKeywordModel::NameKey
                && model->data(index, cwKeywordModel::ValueRole).toString() == name) {
                return qobject_cast<cwVisibilityProxy*>(item->object());
            }
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("cwMarkerSource announces each edit with one ranged signal", "[cwMarkerSource]")
{
    FakeMarkerSource source;
    QSignalSpy resetSpy(&source, &cwMarkerSource::markersReset);
    QSignalSpy insertedSpy(&source, &cwMarkerSource::markersInserted);
    QSignalSpy removedSpy(&source, &cwMarkerSource::markersRemoved);
    QSignalSpy changedSpy(&source, &cwMarkerSource::markersChanged);
    QSignalSpy countSpy(&source, &cwMarkerSource::countChanged);

    const QList<cwMarker> initial = testMarkers(0, 3);
    source.resetMarkers(initial);
    CHECK(resetSpy.count() == 1);
    CHECK(countSpy.count() == 1);
    REQUIRE(source.count() == 3);
    REQUIRE(source.markers().size() == 3);
    for (int i = 0; i < 3; ++i) {
        CHECK(source.marker(i).id == initial.at(i).id);
        CHECK(source.marker(i).position == markerPosition(i));
    }

    // Two markers into the middle: rows 1 and 2.
    const QList<cwMarker> inserted = testMarkers(10, 2);
    source.insertMarkers(1, inserted);
    REQUIRE(insertedSpy.count() == 1);
    CHECK(insertedSpy.at(0).at(0).toInt() == 1);
    CHECK(insertedSpy.at(0).at(1).toInt() == 2);
    CHECK(countSpy.count() == 2);
    REQUIRE(source.count() == 5);
    CHECK(source.marker(1).id == inserted.at(0).id);
    CHECK(source.marker(2).id == inserted.at(1).id);
    CHECK(source.marker(3).id == initial.at(1).id);

    const cwMarker moved = testMarker(20);
    source.replaceMarkers(4, {moved});
    REQUIRE(changedSpy.count() == 1);
    CHECK(changedSpy.at(0).at(0).toInt() == 4);
    CHECK(changedSpy.at(0).at(1).toInt() == 4);
    CHECK(countSpy.count() == 2);
    CHECK(source.marker(4).position == moved.position);

    source.removeMarkers(1, 2);
    REQUIRE(removedSpy.count() == 1);
    CHECK(removedSpy.at(0).at(0).toInt() == 1);
    CHECK(removedSpy.at(0).at(1).toInt() == 2);
    CHECK(countSpy.count() == 3);
    REQUIRE(source.count() == 3);
    CHECK(source.marker(1).id == initial.at(1).id);

    CHECK(resetSpy.count() == 1);
    CHECK(insertedSpy.count() == 1);
}

TEST_CASE("A MarkerView draws one sprite and one delegate per marker after a reset", "[cwMarkerView]")
{
    MarkerFixture f;
    QSignalSpy countSpy(f.view, &cwMarkerView::countChanged);

    const QList<cwMarker> markers = testMarkers(0, 3);
    f.source.resetMarkers(markers);

    CHECK(f.view->count() == 3);
    CHECK(countSpy.count() == 1);
    INFO("sprites " << toString(f.spritePositions()));
    CHECK(f.spritePositions() == positionsOf(markers));
    CHECK(f.sprites()->billboardCount() == 3);
    REQUIRE(f.waitForDelegates(positionsOf(markers)));

    for (int i = 0; i < 3; ++i) {
        INFO("marker " << i);
        cwPositionItem* item = f.delegate(QStringLiteral("m%1").arg(i));
        REQUIRE(item != nullptr);
        CHECK(item->position3D() == markerPosition(i));
        CHECK(item->property("markerView").value<cwMarkerView*>() == f.view);
        CHECK(item->property("selectionManager").value<cwSelectionManager*>() == &f.selection);
        CHECK(item->parentItem() == f.view);
    }

    // A second reset replaces the whole set.
    const QList<cwMarker> replacement = testMarkers(5, 2);
    f.source.resetMarkers(replacement);
    CHECK(f.view->count() == 2);
    CHECK(f.spritePositions() == positionsOf(replacement));
    CHECK(f.waitForDelegates(positionsOf(replacement)));
}

TEST_CASE("A MarkerView adds the markers a source inserts mid-list", "[cwMarkerView]")
{
    MarkerFixture f;
    f.source.resetMarkers(testMarkers(0, 2));
    REQUIRE(f.view->count() == 2);

    const QList<cwMarker> inserted = testMarkers(7, 1);
    f.source.insertMarkers(1, inserted);

    QList<cwMarker> all = testMarkers(0, 2);
    all.append(inserted);
    CHECK(f.view->count() == 3);
    CHECK(f.spritePositions() == positionsOf(all));
    REQUIRE(f.waitForDelegates(positionsOf(all)));

    // Row 1 is the inserted marker, so selecting it selects its delegate.
    cwPositionItem* item = f.delegate(QStringLiteral("m7"));
    REQUIRE(item != nullptr);
    f.view->select(1);
    CHECK(f.selection.selectedItem() == item);
    CHECK(item->property("selected").toBool());
}

TEST_CASE("A MarkerView drops the markers a source removes", "[cwMarkerView]")
{
    MarkerFixture f;
    const QList<cwMarker> markers = testMarkers(0, 4);
    f.source.resetMarkers(markers);
    REQUIRE(f.view->count() == 4);
    REQUIRE(f.keywords.rowCount() == 4);

    f.source.removeMarkers(1, 2);

    const QList<cwMarker> left = {markers.at(0), markers.at(3)};
    CHECK(f.view->count() == 2);
    CHECK(f.spritePositions() == positionsOf(left));
    CHECK(f.sprites()->billboardCount() == 2);
    CHECK(f.drawnTexts(f.titles()) == sortedList({QStringLiteral("Title 0"), QStringLiteral("Title 3")}));
    CHECK(f.keywords.rowCount() == 2);
    CHECK(f.waitForDelegates(positionsOf(left)));
    CHECK(f.delegate(QStringLiteral("m1")) == nullptr);
    CHECK(f.delegate(QStringLiteral("m2")) == nullptr);
}

TEST_CASE("A MarkerView updates a changed marker in place", "[cwMarkerView]")
{
    MarkerFixture f;
    f.source.resetMarkers(testMarkers(0, 2));
    REQUIRE(f.view->count() == 2);
    cwPositionItem* item = f.delegate(QStringLiteral("m1"));
    REQUIRE(item != nullptr);

    cwMarker changed = f.source.marker(1);
    changed.position = kMovedPosition;
    changed.textLines = {{QStringLiteral("Renamed"), kTitleStyle},
                         {QStringLiteral("New detail"), kDetailStyle}};
    changed.delegateProperties = {{kTagProperty, QStringLiteral("renamed")}};
    f.source.replaceMarkers(1, {changed});

    CHECK(f.view->count() == 2);
    // The same delegate follows the marker.
    CHECK(f.delegate(QStringLiteral("renamed")) == item);
    CHECK(item->position3D() == changed.position);
    CHECK(f.spritePositions() == sorted({markerPosition(0), changed.position}));
    CHECK(f.drawnTexts(f.titles()) == sortedList({QStringLiteral("Title 0"), QStringLiteral("Renamed")}));
    CHECK(f.drawnTexts(f.details()) == sortedList({QStringLiteral("Detail 0"), QStringLiteral("New detail")}));
    CHECK(f.delegates().size() == 2);
}

TEST_CASE("A MarkerView draws each text line in the layer of its style", "[cwMarkerView]")
{
    MarkerFixture f;
    f.view->setTextGap(kTextGap);

    cwMarker marker = testMarker(0);
    marker.textLines = {{QStringLiteral("Top"), kTitleStyle},
                        {QStringLiteral("Unstyled"), kMissingStyle},
                        {QString(), kTitleStyle},
                        {QStringLiteral("Bottom"), kDetailStyle}};
    f.source.resetMarkers({marker});

    REQUIRE(f.view->count() == 1);
    // A line without a style, and an empty line, draw nothing.
    CHECK(f.drawnTexts(f.titles()) == QStringList{QStringLiteral("Top")});
    CHECK(f.drawnTexts(f.details()) == QStringList{QStringLiteral("Bottom")});
    CHECK(f.titles() != f.details());

    // The last line sits nearest the sprite; the line above it is higher.
    const auto titleSlots = f.titles()->buildRenderSlots();
    const auto detailSlots = f.details()->buildRenderSlots();
    REQUIRE(titleSlots.size() == 1);
    REQUIRE(detailSlots.size() == 1);
    CHECK(titleSlots.at(0).worldPosition == marker.position);
    CHECK(detailSlots.at(0).pixelOffset.y() > 0.0);
    CHECK(titleSlots.at(0).pixelOffset.y() > detailSlots.at(0).pixelOffset.y());
}

TEST_CASE("Markers sharing a visibility group share one keyword item", "[cwMarkerView]")
{
    MarkerFixture f;
    const QString shared = QStringLiteral("shared");
    f.source.resetMarkers({testMarker(0, shared), testMarker(1, shared), testMarker(2)});

    REQUIRE(f.view->count() == 3);
    REQUIRE(f.keywords.rowCount() == 2);

    cwVisibilityProxy* sharedProxy = proxyNamed(f.keywords, shared);
    REQUIRE(sharedProxy != nullptr);

    cwPositionItem* first = f.delegate(QStringLiteral("m0"));
    cwPositionItem* second = f.delegate(QStringLiteral("m1"));
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    f.selection.setSelectedItem(first);
    sharedProxy->setVisible(false);
    CHECK_FALSE(first->isVisible());
    CHECK_FALSE(second->isVisible());
    CHECK(f.spritePositions() == QList<QVector3D>{markerPosition(2)});
    CHECK(f.drawnTexts(f.titles()) == QStringList{QStringLiteral("Title 2")});
    // Hiding the selected marker closes its popup.
    CHECK(f.selection.selectedItem() == nullptr);
    // Hidden is not removed.
    CHECK(f.view->count() == 3);

    // A marker added to a hidden group starts hidden.
    f.source.insertMarkers(3, {testMarker(3, shared)});
    cwPositionItem* added = f.delegate(QStringLiteral("m3"));
    REQUIRE(added != nullptr);
    CHECK_FALSE(added->isVisible());
    CHECK(f.keywords.rowCount() == 2);

    sharedProxy->setVisible(true);
    CHECK(first->isVisible());
    CHECK(added->isVisible());
    CHECK(f.sprites()->buildInstances().size() == 4);

    // The item goes with the group's last marker.
    f.source.removeMarkers(0, 1);
    CHECK(f.keywords.rowCount() == 2);
    f.source.removeMarkers(1, 1);
    CHECK(f.keywords.rowCount() == 1);
}

TEST_CASE("A MarkerView hands its selection manager to its delegates", "[cwMarkerView]")
{
    cwSelectionManager otherSelection;
    MarkerFixture f;
    f.source.resetMarkers(testMarkers(0, 2));
    REQUIRE(f.view->count() == 2);
    cwPositionItem* item = f.delegate(QStringLiteral("m0"));
    REQUIRE(item != nullptr);

    f.view->select(0);
    REQUIRE(f.selection.selectedItem() == item);

    // Removing the selected marker clears the selection.
    f.source.removeMarkers(0, 0);
    CHECK(f.selection.selectedItem() == nullptr);

    cwPositionItem* remaining = f.delegate(QStringLiteral("m1"));
    REQUIRE(remaining != nullptr);
    f.view->setSelectionManager(&otherSelection);
    CHECK(remaining->property("selectionManager").value<cwSelectionManager*>() == &otherSelection);

    // A row out of range selects nothing.
    f.view->select(kOutOfRangeRow);
    CHECK(otherSelection.selectedItem() == nullptr);
    f.view->select(0);
    CHECK(otherSelection.selectedItem() == remaining);
}

namespace {

struct GrowthCounts {
    int resets = 0;
    int viewCountChanges = 0;
    int keywordRowsInserted = 0;
    int needsRendering = 0;
};

// Resets a fresh view's source to @a markerCount markers and counts what that
// one reset cost.
GrowthCounts resetWith(int markerCount)
{
    MarkerFixture f;
    QSignalSpy resetSpy(&f.source, &cwMarkerSource::markersReset);
    QSignalSpy viewCountSpy(f.view, &cwMarkerView::countChanged);
    QSignalSpy keywordSpy(&f.keywords, &QAbstractItemModel::rowsInserted);
    QSignalSpy renderSpy(&f.scene, &cwScene::needsRendering);

    f.source.resetMarkers(testMarkers(0, markerCount));

    INFO("markers " << markerCount);
    CHECK(f.view->count() == markerCount);
    CHECK(f.sprites()->billboardCount() == markerCount);
    CHECK(f.titles()->billboardCount() == markerCount);
    CHECK(f.details()->billboardCount() == markerCount);
    CHECK(f.keywords.rowCount() == markerCount);
    CHECK(f.delegates().size() == markerCount);

    return {int(resetSpy.count()), int(viewCountSpy.count()), int(keywordSpy.count()),
            int(renderSpy.count())};
}

} // namespace

TEST_CASE("A MarkerView's cost of a reset grows linearly with the markers", "[cwMarkerView]")
{
    const GrowthCounts small = resetWith(kSmallMarkerCount);
    const GrowthCounts large = resetWith(kSmallMarkerCount * kGrowthFactor);

    INFO("view countChanged small " << small.viewCountChanges << " large " << large.viewCountChanges);
    INFO("keyword rowsInserted small " << small.keywordRowsInserted << " large " << large.keywordRowsInserted);
    INFO("needsRendering small " << small.needsRendering << " large " << large.needsRendering);

    CHECK(small.resets == 1);
    CHECK(large.resets == 1);

    CHECK(small.viewCountChanges >= 1);
    CHECK(large.viewCountChanges <= kGrowthFactor * small.viewCountChanges);

    CHECK(small.keywordRowsInserted >= 1);
    CHECK(large.keywordRowsInserted <= kGrowthFactor * small.keywordRowsInserted);

    CHECK(large.needsRendering <= kGrowthFactor * std::max(small.needsRendering, 1));
}

namespace {

constexpr float kDefaultXRayOpacity = 0.3f;
constexpr int kEditedXRayOpacityPercent = 55;
constexpr float kEditedXRayOpacity = 0.55f;

// A level camera: the turntable's pitch 0, where the x-ray ghost draws at the
// base opacity knob. The fixture's identity camera looks straight down, where
// it draws at the overhead knob.
QMatrix4x4 levelViewMatrix()
{
    constexpr float kStraightDownToLevelDegrees = -90.0f;
    QMatrix4x4 view;
    view.rotate(kStraightDownToLevelDegrees, 1.0f, 0.0f, 0.0f);
    return view;
}

void checkDepthMode(const char* layerName, const cwBillboardDepthMode& mode,
                    bool depthTest, bool xRay, float xRayOpacity)
{
    INFO(layerName);
    CHECK(mode.depthTest == depthTest);
    CHECK(mode.xRay == xRay);
    CHECK(mode.xRayOpacity == Catch::Approx(xRayOpacity));
}

void checkMarkerLayers(MarkerFixture& f, bool depthTest, bool xRay, float xRayOpacity)
{
    checkDepthMode("sprites", f.sprites()->depthMode(), depthTest, xRay, xRayOpacity);
    checkDepthMode("titles", f.titles()->depthMode(), depthTest, xRay, xRayOpacity);
    checkDepthMode("details", f.details()->depthMode(), depthTest, xRay, xRayOpacity);
}

} // namespace

TEST_CASE("A MarkerView draws its layers in the marker knobs' depth mode and station labels keep the default",
          "[cwMarkerView]")
{
    auto* settings = cwRenderingSettings::instance();
    REQUIRE(settings != nullptr);
    cwMarkerKnobsReset knobsReset;

    MarkerFixture f;
    // Level, so the ghost draws at the base opacity knob these checks move.
    f.camera.setViewMatrix(levelViewMatrix());
    f.source.resetMarkers(testMarkers(0, 2));
    REQUIRE(f.view->count() == 2);
    REQUIRE(f.titles() != f.details());

    const auto checkStationLabelsKeepTheDefault = [&]() {
        CHECK(f.scene.textBillboardLayer()->depthMode() == cwBillboardDepthMode());
        CHECK(f.scene.spriteBillboardLayer()->depthMode() == cwBillboardDepthMode());
    };

    // The defaults ghost a hidden marker at 30 percent.
    checkMarkerLayers(f, true, true, kDefaultXRayOpacity);
    checkStationLabelsKeepTheDefault();

    settings->setMarkerXRayOpacityPercent(kEditedXRayOpacityPercent);
    checkMarkerLayers(f, true, true, kEditedXRayOpacity);

    settings->setMarkerXRay(false);
    checkMarkerLayers(f, true, false, kEditedXRayOpacity);

    settings->setMarkerDepthTest(false);
    checkMarkerLayers(f, false, false, kEditedXRayOpacity);
    checkStationLabelsKeepTheDefault();

    settings->resetMarkerKnobsToDefaults();
    checkMarkerLayers(f, true, true, kDefaultXRayOpacity);
}

TEST_CASE("A MarkerView built after a knob change starts in the knobs' depth mode", "[cwMarkerView]")
{
    auto* settings = cwRenderingSettings::instance();
    REQUIRE(settings != nullptr);
    cwMarkerKnobsReset knobsReset;
    settings->setMarkerXRay(false);
    settings->setMarkerXRayOpacityPercent(kEditedXRayOpacityPercent);
    REQUIRE_FALSE(settings->markerXRay());

    MarkerFixture f;
    f.source.resetMarkers(testMarkers(0, 1));
    REQUIRE(f.view->count() == 1);

    checkMarkerLayers(f, true, false, kEditedXRayOpacity);
}

namespace {

void checkStationLabelMode(MarkerFixture& f)
{
    const cwBillboardDepthMode stationLabelMode;
    checkMarkerLayers(f, stationLabelMode.depthTest, stationLabelMode.xRay,
                      stationLabelMode.xRayOpacity);
}

} // namespace

TEST_CASE("A MarkerView that does not follow the marker knobs keeps the station-label depth mode",
          "[cwMarkerView]")
{
    auto* settings = cwRenderingSettings::instance();
    REQUIRE(settings != nullptr);
    cwMarkerKnobsReset knobsReset;

    MarkerFixture f;
    f.view->setFollowsMarkerKnobs(false);
    f.source.resetMarkers(testMarkers(0, 2));
    REQUIRE(f.view->count() == 2);
    REQUIRE(f.titles() != f.details());

    SECTION("at the knobs' defaults, x-ray on")
    {
        checkStationLabelMode(f);
    }

    SECTION("with the x-ray opacity edited")
    {
        settings->setMarkerXRayOpacityPercent(kEditedXRayOpacityPercent);
        checkStationLabelMode(f);
    }

    SECTION("with x-ray off")
    {
        settings->setMarkerXRay(false);
        checkStationLabelMode(f);
    }

    SECTION("with the depth test off")
    {
        settings->setMarkerXRay(false);
        settings->setMarkerDepthTest(false);
        checkStationLabelMode(f);
    }

    SECTION("with the depth test off and x-ray on")
    {
        settings->setMarkerDepthTest(false);
        settings->setMarkerXRay(true);
        checkStationLabelMode(f);
    }

    SECTION("at any camera tilt, with its readout at full opacity")
    {
        constexpr double kFullOpacityPercent = 100.0;
        QSignalSpy readoutSpy(f.view, &cwMarkerView::xRayOpacityPercentChanged);

        f.camera.setViewMatrix(levelViewMatrix());
        checkStationLabelMode(f);
        CHECK(f.view->xRayOpacityPercent() == Catch::Approx(kFullOpacityPercent));

        f.camera.setViewMatrix(QMatrix4x4());
        checkStationLabelMode(f);
        CHECK(f.view->xRayOpacityPercent() == Catch::Approx(kFullOpacityPercent));
        CHECK(readoutSpy.count() == 0);
    }
}

TEST_CASE("A MarkerView switches between the knobs' and the station-label depth mode as followsMarkerKnobs changes",
          "[cwMarkerView]")
{
    auto* settings = cwRenderingSettings::instance();
    REQUIRE(settings != nullptr);
    cwMarkerKnobsReset knobsReset;
    settings->setMarkerDepthTest(false);

    MarkerFixture f;
    f.source.resetMarkers(testMarkers(0, 2));
    REQUIRE(f.view->count() == 2);
    REQUIRE(f.view->followsMarkerKnobs());
    REQUIRE(f.sprites()->depthMode() == settings->markerDepthMode());
    REQUIRE(settings->markerDepthMode() != cwBillboardDepthMode());

    QSignalSpy followsSpy(f.view, &cwMarkerView::followsMarkerKnobsChanged);

    f.view->setFollowsMarkerKnobs(false);
    CHECK(followsSpy.count() == 1);
    CHECK_FALSE(f.view->followsMarkerKnobs());
    checkStationLabelMode(f);

    f.view->setFollowsMarkerKnobs(false);
    CHECK(followsSpy.count() == 1);

    f.view->setFollowsMarkerKnobs(true);
    CHECK(followsSpy.count() == 2);
    const cwBillboardDepthMode knobMode = settings->markerDepthMode();
    checkMarkerLayers(f, knobMode.depthTest, knobMode.xRay, knobMode.xRayOpacity);

    f.view->setFollowsMarkerKnobs(false);
    CHECK(followsSpy.count() == 3);
    checkStationLabelMode(f);
}

namespace {

// Draws @a markerCount markers, drains the scene's pending work, and counts the
// frames one marker knob change asks for.
int framesForOneKnobChange(int markerCount)
{
    auto* settings = cwRenderingSettings::instance();
    settings->resetMarkerKnobsToDefaults();

    MarkerFixture f;
    // Level, so the base opacity knob this edits reaches the layers.
    f.camera.setViewMatrix(levelViewMatrix());
    f.source.resetMarkers(testMarkers(0, markerCount));
    INFO("markers " << markerCount);
    CHECK(f.view->count() == markerCount);

    cwRhiScene rhiScene;
    CwRhiSceneTestAccess::synchroize(rhiScene, &f.scene);
    QSignalSpy renderSpy(&f.scene, &cwScene::needsRendering);

    settings->setMarkerXRayOpacityPercent(kEditedXRayOpacityPercent);
    const int frames = int(renderSpy.count());
    settings->resetMarkerKnobsToDefaults();
    return frames;
}

} // namespace

TEST_CASE("A marker knob change asks for frames independent of the marker count", "[cwMarkerView]")
{
    REQUIRE(cwRenderingSettings::instance() != nullptr);
    cwMarkerKnobsReset knobsReset;

    const int small = framesForOneKnobChange(kSmallMarkerCount);
    const int large = framesForOneKnobChange(kSmallMarkerCount * kGrowthFactor);

    INFO("needsRendering small " << small << " large " << large);
    CHECK(small >= 1);
    CHECK(large <= kGrowthFactor * std::max(small, 1));
}
