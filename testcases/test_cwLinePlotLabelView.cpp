//Catch includes
#include <catch2/catch_test_macros.hpp>

//Cavewhere includes
#include "cwLinePlotLabelView.h"
#include "SurveyTreeTestHelper.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwLinePlotManager.h"
#include "cwExternalCenterlineManager.h"
#include "cwStationPositionLookup.h"
#include "cwLabel3dGroup.h"
#include "cwLabel3dItem.h"
#include "cwKeywordItem.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordModel.h"
#include "cwKeyword.h"
#include "cwFutureManagerModel.h"
#include "cwRootData.h"
#include "cwUpdatable.h"

//Test helpers
#include "ExternalCenterlineTestHelpers.h"
#include "TestHelper.h"

//Qt includes
#include <QCoreApplication>
#include <QHash>
#include <QQmlComponent>
#include <QSignalSpy>
#include <QQmlEngine>
#include <QMetaObject>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>
#include <QVector3D>
#include <algorithm>
#include <functional>
#include <memory>

namespace {
// cwLinePlotLabelView::updateGroup creates labels from a QQmlComponent, so the
// view must live inside a QML context.
struct LabelViewHost
{
    QQmlEngine engine;
    std::unique_ptr<QObject> root;
    cwLinePlotLabelView* labelView = nullptr;
};

std::unique_ptr<LabelViewHost> makeLabelViewHost()
{
    auto host = std::make_unique<LabelViewHost>();
    QQmlComponent component(&host->engine);
    component.setData(QByteArrayLiteral(
                          "import QtQuick\n"
                          "import cavewherelib\n"
                          "Item {\n"
                          "  width: 200; height: 200\n"
                          "  LinePlotLabelView { objectName: \"labelView\" }\n"
                          "}\n"),
                      QUrl());
    host->root.reset(component.create());
    REQUIRE(host->root != nullptr);
    host->labelView = host->root->findChild<cwLinePlotLabelView*>(QStringLiteral("labelView"));
    REQUIRE(host->labelView != nullptr);
    return host;
}

cwLabel3dGroup* groupForKeywordItem(cwKeywordItem* item)
{
    return qobject_cast<cwLabel3dGroup*>(item->object());
}

QStringList sortedLabelTexts(cwKeywordItemModel& keywordModel)
{
    QStringList texts;
    for (int i = 0; i < keywordModel.rowCount(); ++i) {
        const cwLabel3dGroup* group = groupForKeywordItem(keywordModel.item(i));
        REQUIRE(group != nullptr);
        for (const cwLabel3dItem& label : group->labels()) {
            texts.append(label.text());
        }
    }
    texts.sort();
    return texts;
}
}

// Regression: issue #434 — removing an empty cave used to crash with a
// use-after-free when a positional list of per-cave groups fell out of sync
// with the region. The view is now keyed per trip (a QHash, no positional
// indexing), so the original desync can't happen; this still guards against a
// crash when a station-position change fires on a cave after a sibling removal.
TEST_CASE("Removing an empty cave before a cave with data does not crash cwLinePlotLabelView",
          "[cwLinePlotLabelView]")
{
    auto host = makeLabelViewHost();
    cwLinePlotLabelView* labelView = host->labelView;

    // The empty cave must be at a lower index than the cave with data:
    // after removal, indexOf(dataCave) drops from 1 to 0, which in the
    // buggy code resolves to the freed group.
    cwCavingRegion region;

    cwCave* emptyCave = new cwCave();
    emptyCave->setName("Cave 2");
    region.addCave(emptyCave);

    cwCave* dataCave = new cwCave();
    dataCave->setName("Cave 1");
    region.addCave(dataCave);

    cwTrip* trip = new cwTrip();
    dataCave->addTrip(trip);

    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);

    cwShot shot;
    shot.setDistance(cwDistanceReading("10.0"));
    shot.setCompass(cwCompassReading("0.0"));
    shot.setClino(cwClinoReading("0.0"));
    chunk->appendShot(cwStation("a1"), cwStation("a2"), shot);

    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(&region);
    plotManager->waitToFinish();

    labelView->setRegion(&region);

    region.removeCave(0);

    // Force the deferred delete so any freed group would be dereferenced by the
    // next station-position update if the view kept a stale reference.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    SurveyTreeTestHelper::setScopedPositions(dataCave, cwStationPositionLookup());
    plotManager->waitToFinish();
}

namespace {
// Builds a region with one cave and two trips that share the tie-in station a2
// (trip A: a1->a2, trip B: a2->a3), solves the line plot so the cave has solved
// station positions, and returns the region (caller owns it via the unique_ptr).
struct TwoTripFixture {
    std::unique_ptr<cwCavingRegion> region;
    std::unique_ptr<cwLinePlotManager> plotManager;
    cwTrip* tripA = nullptr;
    cwTrip* tripB = nullptr;
    cwCave* cave = nullptr;
};

TwoTripFixture makeTwoTripFixture() {
    TwoTripFixture f;
    f.region = std::make_unique<cwCavingRegion>();

    f.cave = new cwCave();
    f.cave->setName(QStringLiteral("Cave 1"));
    f.region->addCave(f.cave);

    auto makeShot = [](const QString& dist, const QString& compass, const QString& clino) {
        cwShot s;
        s.setDistance(cwDistanceReading(dist));
        s.setCompass(cwCompassReading(compass));
        s.setClino(cwClinoReading(clino));
        return s;
    };

    f.tripA = new cwTrip();
    f.tripA->setName(QStringLiteral("Trip A"));
    f.tripA->calibrations()->setAutoDeclination(false);
    cwSurveyChunk* chunkA = new cwSurveyChunk();
    f.tripA->addChunk(chunkA);
    chunkA->appendShot(cwStation("a1"), cwStation("a2"), makeShot("10.0", "0.0", "0.0"));
    f.cave->addTrip(f.tripA);

    f.tripB = new cwTrip();
    f.tripB->setName(QStringLiteral("Trip B"));
    f.tripB->calibrations()->setAutoDeclination(false);
    cwSurveyChunk* chunkB = new cwSurveyChunk();
    f.tripB->addChunk(chunkB);
    chunkB->appendShot(cwStation("a2"), cwStation("a3"), makeShot("10.0", "90.0", "0.0"));
    f.cave->addTrip(f.tripB);

    f.plotManager = std::make_unique<cwLinePlotManager>();
    f.plotManager->setRegion(f.region.get());
    f.plotManager->waitToFinish();

    return f;
}
}

// 3b: the label view registers one keyword item per trip, carrying the trip's
// Type=Line Plot keyword (shared with the line plot geometry), with the trip's
// label group as the setVisible() target.
TEST_CASE("cwLinePlotLabelView registers per-trip station label keyword visibility",
          "[cwLinePlotLabelView][keyword]")
{
    TwoTripFixture f = makeTwoTripFixture();

    // Declared before the QML root so the model outlives the view (as in
    // production, where both belong to cwRootData): the view's destructor
    // removes its items from the model, which must still be alive.
    cwKeywordItemModel keywordModel;

    auto host = makeLabelViewHost();
    cwLinePlotLabelView* labelView = host->labelView;

    labelView->setKeywordItemModel(&keywordModel);
    labelView->setRegion(f.region.get());

    SECTION("one keyword item per trip, each carrying Type=Line Plot over a label group") {
        REQUIRE(keywordModel.rowCount() == 2);

        for (int i = 0; i < keywordModel.rowCount(); ++i) {
            cwKeywordItem* item = keywordModel.item(i);
            cwLabel3dGroup* group = groupForKeywordItem(item);
            REQUIRE(group != nullptr);
            CHECK_FALSE(group->labels().isEmpty());

            const auto keywords = item->keywordModel()->keywords();
            CHECK(keywords.contains(cwKeyword(cwKeywordModel::TypeKey,
                                              QStringLiteral("Line Plot"))));
        }
    }

    SECTION("a shared junction station gets a label in each owning trip's group") {
        int a2Count = 0;
        for (int i = 0; i < keywordModel.rowCount(); ++i) {
            cwLabel3dGroup* group = groupForKeywordItem(keywordModel.item(i));
            REQUIRE(group != nullptr);
            for (const cwLabel3dItem& label : group->labels()) {
                if (label.text() == QStringLiteral("a2")) { a2Count++; }
            }
        }
        // The OR-visibility semantics rely on a2 appearing once per owning trip;
        // the view's shared declutter KD-tree rejects the co-located duplicate.
        CHECK(a2Count == 2);
    }

    SECTION("hiding via the keyword target's generic setVisible slot hides the group") {
        cwLabel3dGroup* group = groupForKeywordItem(keywordModel.item(0));
        REQUIRE(group != nullptr);
        CHECK(group->isVisible());

        // Mirror cwKeywordVisibility::invokeVisible's generic dispatch onto a
        // plain QObject target (the group is neither QQuickItem nor cwRenderObject).
        const bool invoked = QMetaObject::invokeMethod(group, "setVisible",
                                                       Q_ARG(bool, false));
        CHECK(invoked);
        CHECK_FALSE(group->isVisible());
        // The labels survive — only their visibility gate flips.
        CHECK_FALSE(group->labels().isEmpty());
    }

    SECTION("removing a trip drops its label keyword item") {
        REQUIRE(keywordModel.rowCount() == 2);

        f.cave->removeTrip(1); // trip B

        CHECK(keywordModel.rowCount() == 1);
    }
}

// An externally-attached trip owns no cwSurveyChunk, so trip->uniqueStations()
// is empty and the view produced no station labels — the line plot drew but its
// stations were unlabeled. The labels must instead come from the solved cave
// lookup (scope prefix "<tripLabel>."), named with the scope-relative tail that
// cwScopeStationListModel's panel shows.
TEST_CASE("cwLinePlotLabelView shows station labels for an externally attached trip",
          "[cwLinePlotLabelView][Attach]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    const QString attachDir = tempSubdir(tempRoot, QStringLiteral("labels-attach"));
    seedAttachment(attachDir, fixturePath(QStringLiteral("survex_simple.svx")));

    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* attached = addAttachedTrip(cave, QStringLiteral("Attached"));

    cwLinePlotManager manager;
    QHash<QUuid, QString> tripDirs;
    tripDirs.insert(attached->id(), attachDir);
    manager.externalCenterlineManager()->setTripAttachmentDirs(tripDirs);
    manager.setRegion(&region);
    manager.waitToFinish();

    REQUIRE_FALSE(manager.hasSolveError());

    cwKeywordItemModel keywordModel;

    auto host = makeLabelViewHost();
    cwLinePlotLabelView* labelView = host->labelView;

    labelView->setKeywordItemModel(&keywordModel);
    labelView->setRegion(&region);

    // The attached trip is the region's only trip; the view registers a label
    // keyword item only when the trip has labels, so its external stations must
    // produce exactly one row.
    REQUIRE(keywordModel.rowCount() == 1);

    cwLabel3dGroup* group = groupForKeywordItem(keywordModel.item(0));
    REQUIRE(group != nullptr);

    QStringList labelTexts;
    for (const cwLabel3dItem& label : group->labels()) {
        labelTexts.append(label.text());
    }

    // survex_simple.svx: *begin Simple with a1, a2, a3, named with the
    // scope-relative tail (matching the trip's station-list panel).
    CHECK(labelTexts.size() == 3);
    CHECK(labelTexts.contains(QStringLiteral("simple.a1")));
    CHECK(labelTexts.contains(QStringLiteral("simple.a2")));
    CHECK(labelTexts.contains(QStringLiteral("simple.a3")));
}

// A trip two levels down hangs from a Section rather than from the region's
// cave list, so the view has to follow node and trip inserts at every depth to
// label it.
TEST_CASE("cwLinePlotLabelView labels the trips of a depth-2 node",
          "[cwLinePlotLabelView]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));

    cwLinePlotManager plotManager;
    plotManager.setRegion(&region);
    plotManager.waitToFinish();

    cwKeywordItemModel keywordModel;

    auto host = makeLabelViewHost();
    cwLinePlotLabelView* labelView = host->labelView;

    labelView->setKeywordItemModel(&keywordModel);
    labelView->setRegion(&region);
    REQUIRE(keywordModel.rowCount() == 0);

    cwCave* section = new cwCave();
    section->setName(QStringLiteral("Upper"));
    addNativeTripWithShot(section, QStringLiteral("First"),
                          QStringLiteral("a1"), QStringLiteral("a2"));
    cave->addNode(section);
    plotManager.waitToFinish();
    REQUIRE_FALSE(plotManager.hasSolveError());

    REQUIRE(keywordModel.rowCount() == 1);
    CHECK(sortedLabelTexts(keywordModel) == QStringList({QStringLiteral("a1"), QStringLiteral("a2")}));

    addNativeTripWithShot(section, QStringLiteral("Second"),
                          QStringLiteral("a2"), QStringLiteral("a3"));
    plotManager.waitToFinish();
    REQUIRE_FALSE(plotManager.hasSolveError());
    CHECK(keywordModel.rowCount() == 2);
}

// FX.6: a derived trip's labels come from the station list the external-file
// scan fills in, which lands apart from the solve. Renaming a Compass survey
// leaves every station position as it was, so the re-solve moves nothing; the
// view must still hear that the scan replaced the trip's stations.
TEST_CASE("cwLinePlotLabelView labels a Compass .dat's stations after the scan replaces the trip",
          "[cwLinePlotLabelView][Attach][Compass]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString datPath = QDir(sourceDir.path()).filePath(QStringLiteral("rename.dat"));
    const QByteArray original = fileContents(fixturePath(QStringLiteral("compass_simple.dat")));
    overwriteFile(datPath, original);

    auto fixture = makeSavedProject(QStringLiteral("labels-compass-rename"),
                                    QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    cwCavingRegion* region = fixture->project->cavingRegion();

    cwKeywordItemModel keywordModel;

    auto host = makeLabelViewHost();
    cwLinePlotLabelView* labelView = host->labelView;

    // Connected before the attach, as in the app, where the 3D view exists
    // before any source is attached.
    labelView->setKeywordItemModel(&keywordModel);
    labelView->setRegion(region);

    cwSurveyNode* sourceRoot = attachSourceThroughManager(fixture.get(), region->rootNode(), datPath);
    drainPipelines(fixture.get());
    REQUIRE(sourceRoot->tripCount() == 1);

    QByteArray renamed = original;
    renamed.replace("SURVEY NAME: A", "SURVEY NAME: B");
    REQUIRE(renamed != original);
    overwriteFile(datPath, renamed);

    auto future = managerOf(fixture.get())->reloadFromSource(sourceRoot);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    INFO("reload: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture.get());
    REQUIRE_FALSE(fixture->rootData->linePlotManager()->hasSolveError());

    REQUIRE(sourceRoot->tripCount() == 1);
    const cwTrip* trip = sourceRoot->trip(0);
    CHECK(trip->name() == QStringLiteral("B"));

    QStringList solved;
    for (const auto& station : trip->solvedStations()) {
        solved.append(station.first);
    }
    solved.sort();
    const QStringList expected({QStringLiteral("A1"), QStringLiteral("A2"), QStringLiteral("A3")});
    REQUIRE(solved == expected);

    // The native trip has no shots, so the .dat trip is the only labeled one.
    CHECK(keywordModel.rowCount() == 1);
    CHECK(sortedLabelTexts(keywordModel) == expected);
}

namespace {

constexpr int kNodeTrips = 20;
constexpr int kFlatBlocks = 200;
constexpr int kFlatShotsPerBlock = 3;

// A solve registers the label view's items in one insert and the line plot's
// in another.
constexpr int kMaxKeywordInsertsPerSolve = 2;

int labelGroupCount(const cwKeywordItemModel& keywordModel)
{
    int count = 0;
    for (int i = 0; i < keywordModel.rowCount(); ++i) {
        if (groupForKeywordItem(keywordModel.item(i)) != nullptr) {
            ++count;
        }
    }
    return count;
}

} // namespace

TEST_CASE("cwLinePlotLabelView registers a solve's label items in one insert",
          "[cwLinePlotLabelView][MainThreadBudget]")
{
    // The trips hang from a depth-2 node, so the cave and the section both
    // relay the region's position pulse.
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwCave* section = new cwCave();
    section->setName(QStringLiteral("Upper"));
    for (int i = 0; i < kNodeTrips; ++i) {
        addNativeTripWithShot(section, QStringLiteral("Trip %1").arg(i),
                              QStringLiteral("a%1").arg(i), QStringLiteral("a%1").arg(i + 1));
    }
    cave->addNode(section);

    cwKeywordItemModel keywordModel;
    auto host = makeLabelViewHost();
    cwLinePlotLabelView* labelView = host->labelView;
    labelView->setKeywordItemModel(&keywordModel);
    labelView->setRegion(&region);
    REQUIRE(keywordModel.rowCount() == 0);

    QSignalSpy inserted(&keywordModel, &QAbstractItemModel::rowsInserted);

    cwLinePlotManager plotManager;
    plotManager.setRegion(&region);
    plotManager.waitToFinish();
    REQUIRE_FALSE(plotManager.hasSolveError());

    CHECK(keywordModel.rowCount() == kNodeTrips);
    CHECK(topLevelRowsInserted(inserted) == 1);
}

TEST_CASE("A solve of a flat Survex source registers its keyword items in at most two inserts",
          "[LinePlotManager][cwLinePlotLabelView][keyword][MainThreadBudget]")
{
    auto fixture = makeSavedProject(QStringLiteral("keyword-bulk"),
                                    QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    cwCavingRegion* region = fixture->project->cavingRegion();
    cwKeywordItemModel* keywordModel = fixture->rootData->keywordItemModel();
    cwLinePlotManager* linePlot = fixture->rootData->linePlotManager();

    // Declared after the fixture so the view, which removes its items from the
    // root data's model on destruction, goes first.
    auto host = makeLabelViewHost();
    host->labelView->setKeywordItemModel(keywordModel);
    host->labelView->setRegion(region);
    drainPipelines(fixture.get());

    const QString source = writeFlatSurvex(tempSubdir(fixture->tempDir, QStringLiteral("flat")),
                                           kFlatBlocks, kFlatShotsPerBlock);

    QSignalSpy inserted(keywordModel, &QAbstractItemModel::rowsInserted);
    QSignalSpy solves(linePlot, &cwLinePlotManager::stationPositionInTripsChanged);

    attachSourceThroughManager(fixture.get(), region->rootNode(), source);
    drainPipelines(fixture.get());
    REQUIRE_FALSE(linePlot->hasSolveError());

    CHECK(labelGroupCount(*keywordModel) == kFlatBlocks);
    CHECK(keywordModel->rowCount() >= 2 * kFlatBlocks);
    REQUIRE(solves.count() >= 1);
    INFO("solves: " << solves.count() << ", keyword item inserts: " << topLevelRowsInserted(inserted));
    // Only the first solve meets new trips; later solves find every key registered.
    CHECK(topLevelRowsInserted(inserted) <= kMaxKeywordInsertsPerSolve);
}

namespace {

constexpr int kBatchedBlocks = 800;
constexpr int kBatchedShotsPerBlock = 3;
constexpr int kBatchedSolveWaitMs = 120000;
constexpr int kSignatureDigits = 9;
constexpr int kBatchedLabelTurns =
    (kBatchedBlocks + cwLinePlotManager::kLabelBatchSize - 1) / cwLinePlotManager::kLabelBatchSize;

const QString kLinePlotJobName = QStringLiteral("Line plot");

static_assert(cwLinePlotManager::kLabelBatchSize > 0);
static_assert(cwLinePlotManager::kLabelBatchSize < kBatchedBlocks,
              "the fixture must span more than one label batch");

QString labelSignature(const QString& text, const QVector3D& position)
{
    return QStringLiteral("%1@%2,%3,%4")
        .arg(text)
        .arg(position.x(), 0, 'g', kSignatureDigits)
        .arg(position.y(), 0, 'g', kSignatureDigits)
        .arg(position.z(), 0, 'g', kSignatureDigits);
}

QString groupSignature(const cwLabel3dGroup* group)
{
    QStringList labels;
    for (const cwLabel3dItem& label : group->labels()) {
        labels.append(labelSignature(label.text(), label.position()));
    }
    labels.sort();
    return labels.join(QLatin1Char(';'));
}

QString tripSignature(const cwTrip* trip)
{
    QStringList labels;
    for (const auto& station : trip->solvedStations()) {
        labels.append(labelSignature(station.first, station.second));
    }
    labels.sort();
    return labels.join(QLatin1Char(';'));
}

QList<cwLabel3dGroup*> labelGroups(const cwLinePlotLabelView* view)
{
    return view->findChildren<cwLabel3dGroup*>(Qt::FindDirectChildrenOnly);
}

//! The view keeps one group per trip and carries no trip pointer on it, so the
//! oracle compares the multiset of group label sets with the multiset of the
//! trips' solved station lists, which is what labels(trip) turns into labels.
QStringList sortedGroupSignatures(const cwLinePlotLabelView* view)
{
    QStringList signatures;
    for (const cwLabel3dGroup* group : labelGroups(view)) {
        signatures.append(groupSignature(group));
    }
    signatures.sort();
    return signatures;
}

QStringList sortedTripSignatures(const cwCavingRegion* region)
{
    QStringList signatures;
    for (const cwTrip* trip : region->rootNode()->allTrips()) {
        signatures.append(tripSignature(trip));
    }
    signatures.sort();
    return signatures;
}

int labeledGroupCount(const cwLinePlotLabelView* view)
{
    int count = 0;
    for (const cwLabel3dGroup* group : labelGroups(view)) {
        if (!group->labels().isEmpty()) {
            ++count;
        }
    }
    return count;
}

//! Runs a callback once per event-loop turn: each call posts the next one, and
//! a posted event lands in the pass after the one that posted it.
class TurnTicker
{
public:
    explicit TurnTicker(std::function<void()> onTurn) : m_onTurn(std::move(onTurn)) {}

    void start()
    {
        m_running = true;
        post();
    }

    void stop() { m_running = false; }

private:
    void post()
    {
        QMetaObject::invokeMethod(&m_context, [this]() { tick(); }, Qt::QueuedConnection);
    }

    void tick()
    {
        if (m_running) {
            m_onTurn();
            post();
        }
    }

    QObject m_context;
    std::function<void()> m_onTurn;
    bool m_running = false;
};

//! How many label groups changed their label count in each event-loop turn.
struct LabelTurnLog {
    QHash<const cwLabel3dGroup*, qsizetype> counts;
    int maxGroupsChangedInOneTurn = 0;
    int turnsWithChanges = 0;

    void startFrom(const cwLinePlotLabelView* view)
    {
        for (const cwLabel3dGroup* group : labelGroups(view)) {
            counts.insert(group, group->labels().size());
        }
    }

    void record(const cwLinePlotLabelView* view)
    {
        int changed = 0;
        for (const cwLabel3dGroup* group : labelGroups(view)) {
            const qsizetype count = group->labels().size();
            if (counts.value(group, 0) != count) {
                ++changed;
            }
            counts.insert(group, count);
        }
        if (changed > 0) {
            ++turnsWithChanges;
            maxGroupsChangedInOneTurn = std::max(maxGroupsChangedInOneTurn, changed);
        }
    }
};

//! A saved project whose native trip holds n1-n2 and is solved and labeled,
//! with a label view on the root data's keyword model, and a flat Survex
//! source of kBatchedBlocks trips written and ready to attach.
struct BatchedLabelFixture {
    std::unique_ptr<SavedProjectFixture> project;
    // Declared after the project so the view, which removes its items from the
    // root data's keyword model on destruction, goes first.
    std::unique_ptr<LabelViewHost> host;
    cwTrip* native = nullptr;
    QString source;

    cwCavingRegion* region() const { return project->project->cavingRegion(); }
    cwLinePlotManager* linePlot() const { return project->rootData->linePlotManager(); }
    cwFutureManagerModel* futures() const { return project->rootData->futureManagerModel(); }
    cwLinePlotLabelView* labelView() const { return host->labelView; }

    bool idle() const
    {
        return linePlot()->updateState() == cwUpdatable::State::Clean && futures()->rowCount() == 0;
    }
};

std::unique_ptr<BatchedLabelFixture> makeBatchedLabelFixture(const QString& name)
{
    auto fixture = std::make_unique<BatchedLabelFixture>();
    fixture->project = makeSavedProject(name, QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    fixture->host = makeLabelViewHost();
    fixture->labelView()->setKeywordItemModel(fixture->project->rootData->keywordItemModel());
    fixture->labelView()->setRegion(fixture->region());

    fixture->native = addNativeTripWithShot(fixture->project->cave, QStringLiteral("Native"),
                                            QStringLiteral("n1"), QStringLiteral("n2"));
    drainPipelines(fixture->project.get());
    REQUIRE_FALSE(fixture->linePlot()->hasSolveError());
    REQUIRE(labeledGroupCount(fixture->labelView()) == 1);

    fixture->source = writeFlatSurvex(tempSubdir(fixture->project->tempDir, QStringLiteral("flat")),
                                      kBatchedBlocks, kBatchedShotsPerBlock);
    return fixture;
}

int linePlotRow(const cwFutureManagerModel* futures)
{
    for (int row = 0; row < futures->rowCount(); ++row) {
        if (futures->index(row).data(cwFutureManagerModel::NameRole).toString() == kLinePlotJobName) {
            return row;
        }
    }
    return -1;
}

} // namespace

// MT.5: a solve's labels land at most cwLinePlotManager::kLabelBatchSize trips
// per event-loop turn, and the "Line plot" job counts those batches as its
// steps and ends only after the last one.
TEST_CASE("A large solve installs its station labels in batches under the Line plot job",
          "[cwLinePlotLabelView][LinePlotManager][cwFutureManagerModel][MainThreadBudget][LabelBatches]")
{
    auto fixture = makeBatchedLabelFixture(QStringLiteral("label-batches"));
    cwFutureManagerModel* futures = fixture->futures();
    const cwLinePlotLabelView* labelView = fixture->labelView();

    LabelTurnLog turns;
    turns.startFrom(labelView);

    QSignalSpy published(fixture->linePlot(), &cwLinePlotManager::cavernOutputChanged);
    int maxSteps = 0;
    int removals = 0;
    int stepsAtRemoval = -1;
    int progressAtRemoval = -1;
    int labeledAtRemoval = -1;
    QObject context;
    // Before its scan applies, the attach can start a solve of the native trip
    // alone, which is canceled or publishes depending on which of it and the
    // harvest takes cavern first. The job under test is the one that labels
    // the sourced trips, and a job that ends before them counts as none.
    QObject::connect(futures, &QAbstractItemModel::rowsAboutToBeRemoved, &context,
                     [&](const QModelIndex&, int first, int last) {
        for (int row = first; row <= last; ++row) {
            const QModelIndex index = futures->index(row);
            if (labeledGroupCount(labelView) > 1
                && index.data(cwFutureManagerModel::NameRole).toString() == kLinePlotJobName) {
                ++removals;
                stepsAtRemoval = index.data(cwFutureManagerModel::NumberOfStepRole).toInt();
                progressAtRemoval = index.data(cwFutureManagerModel::ProgressRole).toInt();
                labeledAtRemoval = labeledGroupCount(labelView);
            }
        }
    });

    TurnTicker ticker([&]() {
        turns.record(labelView);
        const int row = linePlotRow(futures);
        if (published.count() >= 1 && row >= 0) {
            maxSteps = std::max(maxSteps,
                                futures->index(row).data(cwFutureManagerModel::NumberOfStepRole).toInt());
        }
    });
    ticker.start();

    attachSourceThroughManager(fixture->project.get(), fixture->region()->rootNode(), fixture->source);
    REQUIRE(tryWait(kBatchedSolveWaitMs, [&]() {
        return published.count() >= 1 && fixture->idle()
               && labeledGroupCount(labelView) > kBatchedBlocks;
    }));
    ticker.stop();
    drainPipelines(fixture->project.get());
    turns.record(labelView);

    REQUIRE_FALSE(fixture->linePlot()->hasSolveError());
    const int labeled = labeledGroupCount(labelView);
    CHECK(labeled == kBatchedBlocks + 1);

    // Today's labels(trip) is the oracle: every group carries its trip's
    // solved stations, names and positions both.
    CHECK(sortedGroupSignatures(labelView) == sortedTripSignatures(fixture->region()));

    INFO("turns with label changes: " << turns.turnsWithChanges
         << ", most groups relabeled in one turn: " << turns.maxGroupsChangedInOneTurn);
    CHECK(turns.maxGroupsChangedInOneTurn <= cwLinePlotManager::kLabelBatchSize);
    CHECK(turns.turnsWithChanges >= kBatchedLabelTurns);

    INFO("Line plot steps: " << maxSteps << ", at removal: " << stepsAtRemoval
         << " steps, progress " << progressAtRemoval << ", labeled groups " << labeledAtRemoval);
    CHECK(removals == 1);
    CHECK(maxSteps >= kBatchedLabelTurns);
    CHECK(stepsAtRemoval >= kBatchedLabelTurns);
    CHECK(progressAtRemoval == stepsAtRemoval);
    CHECK(labeledAtRemoval == labeled);
}

// MT.5 with CP.2b's solve-once invariant: an edit that lands while the labels
// are still draining restarts the solve inside the same "Line plot" job, runs
// cavern once more, and leaves the labels of the second solve.
TEST_CASE("A native edit made while a solve's labels drain solves once more inside the same job",
          "[LinePlotManager][SolveOnce][cwLinePlotLabelView][MainThreadBudget][LabelBatches]")
{
    auto fixture = makeBatchedLabelFixture(QStringLiteral("label-batches-restart"));
    cwFutureManagerModel* futures = fixture->futures();
    const cwLinePlotLabelView* labelView = fixture->labelView();

    QSignalSpy published(fixture->linePlot(), &cwLinePlotManager::cavernOutputChanged);
    bool edited = false;
    int jobsAfterEdit = 0;
    QObject context;
    QObject::connect(futures, &QAbstractItemModel::rowsInserted, &context,
                     [&](const QModelIndex&, int first, int last) {
        for (int row = first; row <= last; ++row) {
            if (edited
                && futures->index(row).data(cwFutureManagerModel::NameRole).toString() == kLinePlotJobName) {
                ++jobsAfterEdit;
            }
        }
    });

    // The attach can publish a solve of the native trip alone before its
    // scan applies, depending on which of that solve and the harvest takes
    // cavern first, so the edit is keyed to the labels, not a publish count.
    bool editedMidDrain = false;
    int labeledAtEdit = -1;
    qsizetype publishedAtEdit = -1;
    // Read as the edit's solve publishes, before it installs any label: the
    // labels the canceled drain handed over by then.
    int labeledAtSecondPublish = -1;
    QObject::connect(fixture->linePlot(), &cwLinePlotManager::cavernOutputChanged, &context, [&]() {
        if (edited && labeledAtSecondPublish < 0) {
            labeledAtSecondPublish = labeledGroupCount(labelView);
        }
    });
    TurnTicker ticker([&]() {
        if (edited) {
            return;
        }
        // The native trip was labeled before the attach; the drain is under
        // way once a sourced trip is labeled, and over once all of them are.
        const int labeled = labeledGroupCount(labelView);
        if (labeled > 1) {
            edited = true;
            labeledAtEdit = labeled;
            publishedAtEdit = published.count();
            editedMidDrain = labeled < kBatchedBlocks + 1;
            cwShot shot;
            shot.setDistance(cwDistanceReading(QStringLiteral("10.0")));
            shot.setCompass(cwCompassReading(QStringLiteral("90.0")));
            shot.setClino(cwClinoReading(QStringLiteral("0.0")));
            fixture->native->chunk(0)->appendShot(cwStation(QStringLiteral("n2")),
                                                  cwStation(QStringLiteral("n3")), shot);
        }
    });
    ticker.start();

    attachSourceThroughManager(fixture->project.get(), fixture->region()->rootNode(), fixture->source);
    REQUIRE(tryWait(kBatchedSolveWaitMs, [&]() {
        return edited && published.count() > publishedAtEdit && fixture->idle();
    }));
    ticker.stop();
    drainPipelines(fixture->project.get());
    settleEventLoop(kNothingHappensSettleMs);

    REQUIRE_FALSE(fixture->linePlot()->hasSolveError());
    INFO("labeled groups when the edit landed: " << labeledAtEdit << " of " << kBatchedBlocks + 1
         << ", Line plot jobs started after the edit: " << jobsAfterEdit
         << ", cavern runs: " << published.count() << " (" << publishedAtEdit << " at the edit)"
         << ", labeled groups at the second publish: " << labeledAtSecondPublish);
    CHECK(editedMidDrain);
    // The restart stops the superseded drain after the one batch already
    // queued when the edit landed.
    CHECK(labeledAtSecondPublish >= labeledAtEdit);
    CHECK(labeledAtSecondPublish <= labeledAtEdit + cwLinePlotManager::kLabelBatchSize);
    CHECK(published.count() == publishedAtEdit + 1);
    // The edit's solve rides the job of the solve whose labels were draining.
    CHECK(jobsAfterEdit == 0);

    // The labels are the second solve's: n3 exists only there.
    CHECK(tripSignature(fixture->native).contains(QStringLiteral("n3@")));
    CHECK(labeledGroupCount(labelView) == kBatchedBlocks + 1);
    CHECK(sortedGroupSignatures(labelView) == sortedTripSignatures(fixture->region()));
}

// MT.5: the worker computes the labels of derived nodes' trips from member
// views on its own copy of the region, which has to match what the live
// region computes for nested Survex blocks.
TEST_CASE("A nested Survex source's drained labels match each trip's solved stations",
          "[cwLinePlotLabelView][LinePlotManager][LabelBatches]")
{
    constexpr int kTreeFiles = 7;
    constexpr int kTreeShots = 6;
    constexpr int kTreeChildren = 2;

    auto project = makeSavedProject(QStringLiteral("label-batches-tree"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwCavingRegion* region = project->project->cavingRegion();
    // Declared after the project so the view, which removes its items from the
    // root data's keyword model on destruction, goes first.
    auto host = makeLabelViewHost();
    host->labelView->setKeywordItemModel(project->rootData->keywordItemModel());
    host->labelView->setRegion(region);
    drainPipelines(project.get());

    const QString source = writeSurvexTree(tempSubdir(project->tempDir, QStringLiteral("tree")),
                                           kTreeFiles, kTreeShots, kTreeChildren);
    attachSourceThroughManager(project.get(), region->rootNode(), source);
    drainPipelines(project.get());
    REQUIRE_FALSE(project->rootData->linePlotManager()->hasSolveError());

    int solvedTrips = 0;
    for (const cwTrip* trip : region->rootNode()->allTrips()) {
        if (!trip->solvedStations().isEmpty()) {
            ++solvedTrips;
        }
    }
    // Every file holds its own block and a side block.
    CHECK(solvedTrips >= 2 * kTreeFiles);
    CHECK(labeledGroupCount(host->labelView) == solvedTrips);
    CHECK(sortedGroupSignatures(host->labelView) == sortedTripSignatures(region));
}
