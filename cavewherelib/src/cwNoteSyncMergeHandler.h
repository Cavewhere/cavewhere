#pragma once

#include "cwSyncMergeHandler.h"

class cwNoteSyncMergeHandler final : public cwSyncMergeHandler
{
public:
    QString name() const override;
    cwReconcileMergeResult reconcile(const cwReconcileMergeContext& context) const override;
};
