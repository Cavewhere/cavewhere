/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCavingRegion.h"
#include "cwExternalCenterlineManager.h"
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"

// AsyncFuture
#include <asyncfuture.h>

// Qt
#include <QMetaObject>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUndoStack>

// Std
#include <algorithm>
#include <memory>

namespace {

constexpr int kBooks = 10;
constexpr int kSurveysPerBook = 100;
constexpr int kShotsPerSurvey = 3;
constexpr int kFixEvery = 25;

// The root insert, one pulse per installed batch, and one to spare.
constexpr int kMaxRootPulses = kBooks + 2;

// A thousand copies, the scan and the fill under Debug + ASAN on a loaded
// machine.
constexpr int kFillWaitMs = 300000;

constexpr int kReloadBooks = 2;
constexpr int kReloadSurveysPerBook = 20;
constexpr int kReloadGrownBooks = 3;

constexpr int kGrowthBooks = 2;
constexpr int kGrowthSmallSurveysPerBook = 100;
constexpr int kGrowthLargeSurveysPerBook = 400;
constexpr double kMaxGrowthRatio = 5.0;

using AttachFuture = QFuture<Monad::Result<cwExternalCenterlineAttach::AttachReport>>;

std::unique_ptr<SavedProjectFixture> makeProject(const QString& projectFileBase)
{
    return makeSavedProject(projectFileBase, QStringLiteral("NativeCave"),
                            QStringLiteral("NativeTrip"));
}

cwSurveyNode* regionRoot(SavedProjectFixture* fixture)
{
    return fixture->project->cavingRegion()->rootNode();
}

QString writeProject(const QTemporaryDir& dir, int books, int surveysPerBook)
{
    return writeWallsProject(tempSubdir(dir, QStringLiteral("walls")), books, surveysPerBook,
                             kShotsPerSurvey, kFixEvery);
}

QString attachJobName()
{
    return QStringLiteral("Attaching %1").arg(generatedWallsProject::kEntryFileName);
}

void waitForAttach(const AttachFuture& future)
{
    REQUIRE(AsyncFuture::waitForFinished(future, kFillWaitMs));
}

void requireSucceeded(const AttachFuture& future)
{
    INFO("attach: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
}

//! What cwFutureManagerModel showed of the attach's own row.
struct AttachJobWatch {
    QPersistentModelIndex row;
    bool listed = false;
    bool removed = false;
    int steps = 0;
    int progressChanges = 0;
    int highestProgress = 0;
};

void watchAttachJob(AttachJobWatch* watch, cwFutureManagerModel* model, QObject* context)
{
    const QString name = attachJobName();
    QObject::connect(model, &QAbstractItemModel::rowsInserted, context,
                     [watch, model, name](const QModelIndex&, int first, int last) {
        for (int row = first; row <= last; ++row) {
            const QModelIndex index = model->index(row);
            if (index.data(cwFutureManagerModel::NameRole).toString() == name) {
                watch->row = QPersistentModelIndex(index);
                watch->listed = true;
                watch->steps = index.data(cwFutureManagerModel::NumberOfStepRole).toInt();
            }
        }
    });
    QObject::connect(model, &QAbstractItemModel::dataChanged, context,
                     [watch](const QModelIndex& topLeft, const QModelIndex& bottomRight,
                             const QList<int>& roles) {
        if (!watch->row.isValid() || !roles.contains(cwFutureManagerModel::ProgressRole)) {
            return;
        }
        const int row = watch->row.row();
        if (row < topLeft.row() || row > bottomRight.row()) {
            return;
        }
        ++watch->progressChanges;
        watch->highestProgress = std::max(
            watch->highestProgress, watch->row.data(cwFutureManagerModel::ProgressRole).toInt());
    });
    QObject::connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, context,
                     [watch](const QModelIndex&, int first, int last) {
        if (watch->row.isValid() && watch->row.row() >= first && watch->row.row() <= last) {
            watch->removed = true;
        }
    });
}

//! Calls \a onPartial once, at the first pulse of the attach's root that shows
//! some of its books and not all of them: a moment in the middle of the fill.
void onPartialTree(cwSurveyNode* parent, int books, QObject* context,
                   const std::function<void(cwSurveyNode* root)>& onPartial)
{
    auto fired = std::make_shared<bool>(false);
    QObject::connect(parent, &cwSurveyNode::insertedNodes, context,
                     [parent, books, context, onPartial, fired](int begin, int) {
        cwSurveyNode* root = parent->childNode(begin);
        QObject::connect(root, &cwSurveyNode::subtreeChanged, context,
                         [root, books, onPartial, fired]() {
            const int count = root->childNodeCount();
            if (*fired || count == 0 || count >= books) {
                return;
            }
            *fired = true;
            onPartial(root);
        });
    });
}

//! Checks the tree writeWallsProject(\a books, \a surveysPerBook) attaches as:
//! one WallsBook per book, in order, each holding its surveys as trips.
void checkGeneratedShape(const cwSurveyNode* root, int books, int surveysPerBook)
{
    using namespace generatedWallsProject;
    CHECK(root->kind() == cwSurveyNode::Kind::WallsBook);
    CHECK(root->isSourceRoot());
    CHECK(root->tripCount() == 0);

    QStringList expectedBooks;
    for (int book = 0; book < books; ++book) {
        expectedBooks.append(bookTitle(book));
    }
    REQUIRE(childNodeNames(root) == expectedBooks);

    for (int book = 0; book < books; ++book) {
        const cwSurveyNode* node = root->childNode(book);
        INFO("book " << node->name().toStdString());
        CHECK(node->kind() == cwSurveyNode::Kind::WallsBook);
        CHECK(node->sourcePath() == bookName(book));
        CHECK(node->sourceRoot() == root);
        CHECK(node->isReadOnly());
        CHECK(node->childNodeCount() == 0);
        REQUIRE(node->tripCount() == surveysPerBook);
        for (int k = 0; k < surveysPerBook; ++k) {
            const int survey = book * surveysPerBook + k;
            const cwTrip* trip = node->trip(k);
            INFO("survey " << survey);
            CHECK(trip->name() == surveyTitle(survey));
            CHECK(trip->sourcePath() == bookName(book) + QStringLiteral("/") + surveyName(survey));
            CHECK(trip->date().date() == surveyDate(survey));
        }
    }
}

//! The max event-loop gap from the attach call until its future finishes.
qint64 attachStall(int surveysPerBook)
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeProject(sourceDir, kGrowthBooks, surveysPerBook);
    auto fixture = makeProject(QStringLiteral("fill-growth-%1").arg(surveysPerBook));
    drainPipelines(fixture.get());

    AttachFuture future;
    const qint64 stall = longestMainThreadStallUntil(
        [&]() { future = managerOf(fixture.get())->attachSource(regionRoot(fixture.get()), entry); },
        [&]() { return future.isFinished(); });
    requireSucceeded(future);
    drainPipelines(fixture.get());
    return stall;
}

} // namespace

TEST_CASE("Attaching a large Walls project installs its tree in batches under one task-list row",
          "[Attach][AttachFill]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeProject(sourceDir, kBooks, kSurveysPerBook);

    auto fixture = makeProject(QStringLiteral("fill-walls"));
    drainPipelines(fixture.get());
    QUndoStack* undoStack = fixture->project->undoStack();
    REQUIRE(undoStack != nullptr);
    const int undoCountBefore = undoStack->count();

    QObject context;
    AttachJobWatch job;
    watchAttachJob(&job, fixture->rootData->futureManagerModel(), &context);
    QSignalSpy rootPulses(regionRoot(fixture.get()), &cwSurveyNode::subtreeChanged);

    auto future = managerOf(fixture.get())->attachSource(regionRoot(fixture.get()), entry);
    waitForAttach(future);
    const int pulses = rootPulses.count();
    requireSucceeded(future);
    cwSurveyNode* root = future.result().value().root.data();
    REQUIRE(root != nullptr);

    // The tree lands a batch at a time, not a row at a time.
    CHECK(pulses <= kMaxRootPulses);

    // The task list carries the fill as one row with a range that advances
    // and goes away once the last batch lands.
    CHECK(job.listed);
    CHECK(job.steps > 0);
    drainPipelines(fixture.get());
    CHECK(job.progressChanges >= 1);
    CHECK(job.highestProgress > 0);
    CHECK(job.removed);

    checkGeneratedShape(root, kBooks, kSurveysPerBook);

    // Undo is deferred to a future version, so the attach records no undo step.
    CHECK(undoStack->count() == undoCountBefore);
    drainPipelines(fixture.get());
}

TEST_CASE("Reloading a grown Walls project records no undo step", "[Attach][AttachFill]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString walls = tempSubdir(sourceDir, QStringLiteral("walls"));
    writeWallsProject(walls, kReloadBooks, kReloadSurveysPerBook, kShotsPerSurvey, kFixEvery);

    auto fixture = makeProject(QStringLiteral("fill-reload"));
    cwSurveyNode* root = attachSourceThroughManager(
        fixture.get(), regionRoot(fixture.get()),
        QDir(walls).filePath(generatedWallsProject::kEntryFileName));
    drainPipelines(fixture.get());
    REQUIRE(root->childNodeCount() == kReloadBooks);

    // A third book joins; the first two keep their surveys.
    writeWallsProject(walls, kReloadGrownBooks, kReloadSurveysPerBook, kShotsPerSurvey, kFixEvery);
    QUndoStack* undoStack = fixture->project->undoStack();
    const int undoCountBefore = undoStack->count();

    auto future = managerOf(fixture.get())->reloadFromSource(root);
    waitForAttach(future);
    requireSucceeded(future);
    drainPipelines(fixture.get());

    checkGeneratedShape(root, kReloadGrownBooks, kReloadSurveysPerBook);
    CHECK(undoStack->count() == undoCountBefore);
}

TEST_CASE("A source root that is still filling refuses Replace and Delete",
          "[Attach][AttachFill]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeProject(sourceDir, kBooks, kSurveysPerBook);

    auto fixture = makeProject(QStringLiteral("fill-busy"));
    drainPipelines(fixture.get());
    cwExternalCenterlineManager* manager = managerOf(fixture.get());

    AttachFuture future;
    bool probedMidFill = false;
    bool rootBusy = false;
    QString replaceRefusal;
    QString deleteRefusal;
    QObject context;
    onPartialTree(regionRoot(fixture.get()), kBooks, &context, [&](cwSurveyNode* root) {
        QPointer<cwSurveyNode> rootPointer(root);
        QMetaObject::invokeMethod(&context, [&, rootPointer]() {
            probedMidFill = !future.isFinished();
            if (!probedMidFill || rootPointer.isNull()) {
                return;
            }
            rootBusy = manager->isOwnerBusy(rootPointer->id());
            const auto replaceFuture = manager->replaceSource(rootPointer, entry);
            if (replaceFuture.isFinished()) {
                replaceRefusal = replaceFuture.result().errorMessage();
            }
            const auto deleteFuture = manager->deleteSourceRoot(rootPointer);
            if (deleteFuture.isFinished()) {
                deleteRefusal = deleteFuture.result().errorMessage();
            }
        }, Qt::QueuedConnection);
    });

    future = manager->attachSource(regionRoot(fixture.get()), entry);
    waitForAttach(future);
    drainPipelines(fixture.get());

    CHECK(probedMidFill);
    CHECK(rootBusy);
    CHECK(replaceRefusal.contains(QStringLiteral("in progress")));
    CHECK(deleteRefusal.contains(QStringLiteral("in progress")));

    // The fill the refusals left alone runs to the end.
    requireSucceeded(future);
    checkGeneratedShape(future.result().value().root.data(), kBooks, kSurveysPerBook);
}

TEST_CASE("Closing the project mid-fill ends the attach and leaves nothing behind",
          "[Attach][AttachFill]")
{
    QTemporaryDir sourceDir;
    REQUIRE(sourceDir.isValid());
    const QString entry = writeProject(sourceDir, kBooks, kSurveysPerBook);

    auto fixture = makeProject(QStringLiteral("fill-close"));
    drainPipelines(fixture.get());
    cwSurveyNode* parent = regionRoot(fixture.get());

    AttachFuture future;
    bool closedMidFill = false;
    QPointer<cwSurveyNode> rootPointer;
    QObject context;

    SECTION("through Close Project") {
        onPartialTree(parent, kBooks, &context, [&](cwSurveyNode* root) {
            rootPointer = root;
            QMetaObject::invokeMethod(&context, [&]() {
                closedMidFill = !future.isFinished();
                fixture->project->newProject();
            }, Qt::QueuedConnection);
        });

        future = managerOf(fixture.get())->attachSource(parent, entry);
        waitForAttach(future);
        CHECK(closedMidFill);
        CHECK((future.isCanceled() || future.result().hasError()));

        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        settleEventLoop(kNothingHappensSettleMs);
        CHECK(parent->childNodeCount() == 0);
        CHECK(rootPointer.isNull());
        CHECK(tryWait(kFillWaitMs, [&fixture]() {
            return fixture->rootData->futureManagerModel()->rowCount() == 0;
        }));
        drainPipelines(fixture.get());
    }

    SECTION("by destroying the project") {
        onPartialTree(parent, kBooks, &context, [&](cwSurveyNode* root) {
            rootPointer = root;
            QMetaObject::invokeMethod(&context, [&]() {
                closedMidFill = !future.isFinished();
                fixture->rootData.reset();
            }, Qt::QueuedConnection);
        });

        future = managerOf(fixture.get())->attachSource(parent, entry);
        REQUIRE(tryWait(kFillWaitMs, [&fixture]() { return fixture->rootData == nullptr; }));
        CHECK(closedMidFill);
        CHECK(tryWait(kFillWaitMs, [&future]() { return future.isFinished(); }));

        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        settleEventLoop(kNothingHappensSettleMs);
        CHECK(rootPointer.isNull());
    }
}

TEST_CASE("The attach's longest event-loop stall grows no faster than its batch",
          "[Attach][AttachFill]")
{
    const qint64 small = attachStall(kGrowthSmallSurveysPerBook);
    const qint64 large = attachStall(kGrowthLargeSurveysPerBook);
    INFO("longest stall: " << small << " ms at " << kGrowthSmallSurveysPerBook << " surveys per book, "
         << large << " ms at " << kGrowthLargeSurveysPerBook);
    REQUIRE(small > 0);
    CHECK(static_cast<double>(large) / static_cast<double>(small) <= kMaxGrowthRatio);
}
