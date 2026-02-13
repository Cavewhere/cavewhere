#pragma once

#include "cwScrapSyncMergeHandler.h"
#include "Monad/Result.h"

class cwScrapMergeApplier
{
public:
    static Monad::Result<cwScrapMergeApplyResult> applyNoteStructuralMergePlan(const cwNoteStructuralMergePlan& plan);
};
