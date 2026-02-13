#pragma once

#include "cwSyncMergeHandler.h"

class cwCaveSyncMergeHandler final : public cwSyncMergeHandler
{
public:
    QString name() const override;
    cwReconcileMergeResult reconcile(const cwReconcileMergeContext& context) const override;
};
