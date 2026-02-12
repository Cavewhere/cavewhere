#pragma once

#include "cwSyncMergeHandler.h"

#include <memory>
#include <vector>

class cwSyncMergeRegistry
{
public:
    static const cwSyncMergeRegistry& instance();

    cwReconcileMergeResult reconcile(const cwReconcileMergeContext& context) const;

private:
    cwSyncMergeRegistry();

    std::vector<std::unique_ptr<cwSyncMergeHandler>> m_handlers;
};
