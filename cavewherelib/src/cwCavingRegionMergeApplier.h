#pragma once

#include "CaveWhereLibExport.h"
#include "Monad/Result.h"
#include "cwCavingRegionMergePlanBuilder.h"

class CAVEWHERE_LIB_EXPORT cwCavingRegionMergeApplier
{
public:
    static Monad::ResultBase applyRegionMergePlan(const cwCavingRegionMergePlan& plan);

    //! Three-way merge of two sides' source lists by id. A source either side
    //! added is kept; a source one side removed from the base is dropped. With
    //! no base, every source of both sides is kept. Rows keep their side's
    //! order with ours appended, so a merge that adds nothing of ours
    //! reproduces theirs exactly.
    static QList<cwExternalSource> mergeExternalSources(
        const QList<cwExternalSource>& ours,
        const QList<cwExternalSource>& theirs,
        const std::optional<QList<cwExternalSource>>& base);
};
