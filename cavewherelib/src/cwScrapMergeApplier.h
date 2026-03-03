#pragma once

#include "CaveWhereLibExport.h"
#include "cwScrapSyncMergeHandler.h"
#include "Monad/Result.h"

class CAVEWHERE_LIB_EXPORT cwScrapMergeApplier
{
public:
    static Monad::Result<cwScrapMergeApplyResult> applyNoteStructuralMergePlan(const cwNoteStructuralMergePlan& plan);
};
