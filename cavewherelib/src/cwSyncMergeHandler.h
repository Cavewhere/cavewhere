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
