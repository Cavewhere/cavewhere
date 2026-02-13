#pragma once

#include "cwSyncMergeHandler.h"

class cwTripSyncMergeHandler final : public cwSyncMergeHandler
{
public:
    QString name() const override;
    cwReconcileMergeResult reconcile(const cwReconcileMergeContext& context) const override;
};

