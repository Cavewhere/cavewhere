// Catch
#include <catch2/catch_test_macros.hpp>

// Std
#include <functional>
#include <memory>

// Qt
#include <QAbstractItemModel>
#include <QDir>
#include <QModelIndex>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUrl>

// Ours
#include "Account.h"
#include "GitRepository.h"
#include "LoadProjectHelper.h"
#include "MockAuthProvider.h"
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwRootData.h"

// libgit2
#include "git2.h"

namespace {

// Minimal project wired to a file:// bare remote, mirroring the InstallGateFixture
// used by the install-check tests: sync() reaches the install-check gate without
// needing HTTPS credentials.
struct SyncJobFixture {
    std::unique_ptr<cwRootData> rootData;
    cwProject* project = nullptr;
    QTemporaryDir projectDir;
    QTemporaryDir remoteRoot;
    QString remoteRepoPath;

    SyncJobFixture() {
        rootData = std::make_unique<cwRootData>();
        project = rootData->project();
        rootData->account()->setName(QStringLiteral("Sync Job Tester"));
        rootData->account()->setEmail(QStringLiteral("sync.job@example.com"));

        REQUIRE(projectDir.isValid());
        const QString projectPath = QDir(projectDir.path()).filePath(QStringLiteral("sync-jobs.cwproj"));
        REQUIRE(project->saveAs(projectPath));
        project->waitSaveToFinish();

        REQUIRE(remoteRoot.isValid());
        remoteRepoPath = QDir(remoteRoot.path()).filePath(QStringLiteral("remote.git"));
        REQUIRE(initBareRepo(remoteRepoPath) == GIT_OK);
        const QString addRemoteError = project->repository()->addRemote(
            QStringLiteral("origin"), QUrl::fromLocalFile(remoteRepoPath));
        REQUIRE(addRemoteError.isEmpty());
    }

    // Every "Syncing" row inserted into the future manager during the capture window.
    QStringList captureSyncingJobs(cwFutureManagerModel* model, const std::function<void()>& action) {
        QStringList seen;
        auto connection = QObject::connect(
            model, &QAbstractItemModel::rowsInserted, model,
            [&](const QModelIndex& parent, int first, int last) {
                Q_UNUSED(parent);
                for (int row = first; row <= last; ++row) {
                    const QString name = model->data(model->index(row, 0),
                                                     cwFutureManagerModel::NameRole).toString();
                    if (name == QStringLiteral("Syncing")) {
                        seen.append(name);
                    }
                }
            });
        action();
        QObject::disconnect(connection);
        return seen;
    }

    // The highest progressMaximum surfaced on any "Syncing" row while `action`
    // runs. Nonzero proves the sync operation's live progress reaches the job
    // through the whole track() chain (operation queue -> syncDeferred -> cycle).
    int captureMaxSyncingProgressRange(cwFutureManagerModel* model, const std::function<void()>& action) {
        int maxRange = 0;
        auto connection = QObject::connect(
            model, &QAbstractItemModel::dataChanged, model,
            [&](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles) {
                Q_UNUSED(roles);
                for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
                    const QModelIndex jobIndex = model->index(row, 0);
                    const QString name = model->data(jobIndex, cwFutureManagerModel::NameRole).toString();
                    if (name != QStringLiteral("Syncing")) {
                        continue;
                    }
                    maxRange = std::max(maxRange,
                                        model->data(jobIndex, cwFutureManagerModel::NumberOfStepRole).toInt());
                }
            });
        action();
        QObject::disconnect(connection);
        return maxRange;
    }
};

} // namespace

// A single Sync click must register exactly one "Syncing" job in the future
// manager model (the UI shows one row per job). Today the credential gate,
// the install-check gate, and the actual sync operation each reassign
// cwProject::SyncFuture and call FutureToken.addJob(..., "Syncing"), so every
// gate that fires adds another identical row. With a file:// remote the
// credential gate is skipped, so the install gate + operation produce TWO rows;
// on an HTTPS (GitHub) remote all three fire and the user sees THREE. Either way
// this is the "multiple sync jobs" misbehavior.
TEST_CASE("cwProject::sync() adds exactly one Syncing job to the future manager",
          "[cwProject][sync][jobs][regression]")
{
    SyncJobFixture fx;

    auto stub = std::make_unique<MockAuthProvider>();
    stub->setInstalled(true);
    fx.project->setAuthProvider(stub.get());

    auto* futureModel = fx.rootData->futureManagerModel();

    QStringList syncingJobs = fx.captureSyncingJobs(futureModel, [&]() {
        REQUIRE(fx.project->sync());
        futureModel->waitForFinished();
        fx.project->waitSaveToFinish();
    });

    INFO("Syncing jobs inserted for one sync(): " << syncingJobs.size());
    CHECK(stub->verifyCalls() == 1);
    CHECK(syncingJobs.size() == 1);
}

// The "Syncing" job's progress bar must advance during the sync: the underlying
// pull/push operations report byte progress, and that progress must survive the
// operation queue, cwSaveLoad's syncDeferred, and cwProject's SyncCycle to reach
// the future-manager row. A flat (progressMaximum == 0) bar is the "flat progress"
// bug this guards against. The first sync pushes local commits to the empty bare
// remote, so libgit2's push_transfer_progress reports a nonzero range.
TEST_CASE("cwProject::sync() surfaces operation progress on the Syncing job",
          "[cwProject][sync][jobs][progress]")
{
    SyncJobFixture fx;

    auto stub = std::make_unique<MockAuthProvider>();
    stub->setInstalled(true);
    fx.project->setAuthProvider(stub.get());

    auto* futureModel = fx.rootData->futureManagerModel();

    const int maxRange = fx.captureMaxSyncingProgressRange(futureModel, [&]() {
        REQUIRE(fx.project->sync());
        futureModel->waitForFinished();
        fx.project->waitSaveToFinish();
    });

    INFO("Max progressMaximum surfaced on the Syncing job: " << maxRange);
    CHECK(maxRange > 0);
}
