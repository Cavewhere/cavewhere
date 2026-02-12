#pragma once

#include "cwSaveLoad.h"

#include <QDir>
#include <QList>
#include <QString>

class cwCavingRegion;
class QObject;

struct cwReconcileMergeContext {
    cwSaveLoad* saveLoad = nullptr;
    cwCavingRegion* region = nullptr;
    const cwSaveLoad::ProjectLoadData* loadData = nullptr;
    const cwSaveLoad::SyncReport* report = nullptr;
    QDir repoRoot;
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
};

class cwSyncMergeHandler
{
public:
    virtual ~cwSyncMergeHandler() = default;
    virtual QString name() const = 0;
    virtual cwReconcileMergeResult reconcile(const cwReconcileMergeContext& context) const = 0;
};
