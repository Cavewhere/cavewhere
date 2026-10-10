#pragma once

#include "CaveWhereLibExport.h"
#include "Monad/Result.h"
#include "cwCavingRegionMergePlanBuilder.h"

class CAVEWHERE_LIB_EXPORT cwCavingRegionMergeApplier
{
public:
    static Monad::ResultBase applyRegionMergePlan(const cwCavingRegionMergePlan& plan);

    //! Three-way merge of two sides' source lists by id. A source either side
    //! added is kept; a source one side removed from the base is dropped. Each
    //! kept source's exclusions merge the same way, so an item either side hid
    //! stays hidden and an item one side restored comes back. With no base,
    //! every source and every exclusion of both sides is kept. Rows and
    //! exclusions keep their side's order with ours appended, so a merge that
    //! adds nothing of ours reproduces theirs exactly.
    static QList<cwExternalSource> mergeExternalSources(
        const QList<cwExternalSource>& ours,
        const QList<cwExternalSource>& theirs,
        const std::optional<QList<cwExternalSource>>& base);
};
