#pragma once

#include "cwSaveLoad.h"

#include <QDir>
#include <QList>
#include <QString>
#include <QStringList>

#include <algorithm>

class cwCavingRegion;
class QObject;

enum class cwReconcileApplyMode {
    Merge,
    TargetCommitWins
};

struct cwReconcileMergeContext {
    cwSaveLoad* saveLoad = nullptr;
    cwCavingRegion* region = nullptr;
    const cwSaveLoad::ProjectLoadData* loadData = nullptr;
    const cwSaveLoad::SyncReport* report = nullptr;
    cwReconcileApplyMode applyMode = cwReconcileApplyMode::Merge;
    QDir repoRoot;
    // Set to true by cwCavingRegionSyncMergeHandler when a git pull moved the
    // project directory. All other handlers should then set diskAlreadySynchronized=true
    // because git already placed the files in the correct locations on disk.
    mutable bool gitProjectDirMoved = false;
    // Node directories (repository-root relative) cwSurveyNodeSyncMergeHandler restored
    // from our pre-merge commit after a peer's delete lost to a local change inside them.
    // Our side won that whole subtree, so every changed path below one is already settled:
    // handlers merge mergeablePaths() rather than the report's raw changedPaths.
    mutable QStringList locallyRestoredNodeDirectories;

    QString dataRootName() const
    {
        if (loadData != nullptr && !loadData->metadata.dataRoot.isEmpty()) {
            return loadData->metadata.dataRoot;
        }
        return saveLoad != nullptr ? saveLoad->dataRoot() : QString();
    }

    bool isUnderRestoredNodeDirectory(const QString& relativePath) const
    {
        const QString cleanPath = QDir::cleanPath(QDir::fromNativeSeparators(relativePath));
        return std::any_of(locallyRestoredNodeDirectories.cbegin(),
                           locallyRestoredNodeDirectories.cend(),
                           [&cleanPath](const QString& restoredDir) {
            return cleanPath.startsWith(restoredDir + QLatin1Char('/'));
        });
    }

    //! The changed paths a handler merges: everything the pull touched, minus the
    //! subtrees a local change already won whole.
    QStringList mergeablePaths() const
    {
        if (report == nullptr) {
            return {};
        }
        if (locallyRestoredNodeDirectories.isEmpty()) {
            return report->changedPaths;
        }

        QStringList paths;
        paths.reserve(report->changedPaths.size());
        for (const QString& changedPath : report->changedPaths) {
            if (!isUnderRestoredNodeDirectory(changedPath)) {
                paths.append(changedPath);
            }
        }
        return paths;
    }
};

//! Files (repository-root relative) to write back from \a commit because a local change
//! won their whole subtree. A handler reports the write rather than performing it: the
//! registry discards its result when a later handler asks for a full reload, and files
//! already written would outlive that decision.
struct cwRestoreFilesFromCommit {
    QString commit;
    QStringList relativePaths;
};

struct cwReconcileMergeResult {
    enum class Outcome {
        NotApplicable,
        Applied,
        RequiresFullReload
    };

    Outcome outcome = Outcome::NotApplicable;
    QString handlerName;
    QString fallbackReason;
    bool modelMutated = false;
    // Set to true when the reconcile handler already ensured disk state matches the
    // in-memory model (e.g. git renamed all files during pull). When true,
    // requiresPersistence is suppressed even if modelMutated is true, preventing an
    // unnecessary "Sync Reconcile" commit.
    bool diskAlreadySynchronized = false;
    // Set to true when conflicting project files (a peer's .cwproj + data directory) were
    // left on disk by a git merge and must be deleted. Forces a cleanup commit even when
    // diskAlreadySynchronized is true for all other handlers.
    bool pendingConflictCleanup = false;
    // Set by a handler that merged note or LiDAR-note descriptors into the model, so the
    // reconcile save writes those descriptors back out. Handlers report it here rather
    // than cwSaveLoad matching handler names, which the registry joins into one string.
    bool persistNoteDescriptors = false;
    bool persistLiDARNoteDescriptors = false;
    // Directories (repository-root relative) a handler found orphaned by a merge and wants
    // removed. Handlers record them here instead of queuing the removal themselves, so a
    // later handler's RequiresFullReload discards them along with the rest of the result.
    QStringList orphanDirectoriesToRemove;
    // Subtrees a handler wants written back from a commit, applied by cwSaveLoad once the
    // registry settled on Applied.
    QList<cwRestoreFilesFromCommit> filesToRestore;
    QList<QObject*> objectsPathReady;
    QStringList diagnostics;
};

class cwSyncMergeHandler
{
public:
    virtual ~cwSyncMergeHandler() = default;
    virtual QString name() const = 0;
    virtual cwReconcileMergeResult reconcile(const cwReconcileMergeContext& context) const = 0;
};
