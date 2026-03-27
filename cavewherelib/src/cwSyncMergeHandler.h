#pragma once

#include "cwSaveLoad.h"

#include <QDir>
#include <QList>
#include <QString>

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

    QString dataRootName() const
    {
        if (loadData != nullptr && !loadData->metadata.dataRoot.isEmpty()) {
            return loadData->metadata.dataRoot;
        }
        return saveLoad != nullptr ? saveLoad->dataRoot() : QString();
    }
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
