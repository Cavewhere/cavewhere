/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwAppearanceSlotted.h"
#include "cwRHIObject.h"

// Qt includes
#include <QtGlobal>
#include <rhi/qrhi.h>

// Std includes
#include <algorithm>

cwAppearanceSlotted::~cwAppearanceSlotted()
{
    qDeleteAll(m_retiredAppearanceResources);
}

void cwAppearanceSlotted::reserveAppearanceSlots(QRhi* rhi, QRhiResourceUpdateBatch* batch,
                                                 int slotCount)
{
    slotCount = std::min(slotCount, cwRHIObject::kAppearanceSlotCount);
    if (slotCount > m_appearanceSlotCapacity) {
        resizeAppearanceSlots(rhi, batch, slotCount);
    }
}

int cwAppearanceSlotted::acquireAppearanceSlot(QRhi* rhi, QRhiResourceUpdateBatch* batch)
{
    // The appearance ceiling (kAppearanceSlotCount) is intentionally below the
    // camera-slot batch cap, so a batch could in principle ask for more concurrent
    // overrides of one object than the budget allows. Enforce it at runtime (not
    // just via assert, which a release build drops): degrade to "no slot" so the
    // caller renders the object live rather than growing the UBO past the budget.
    if (m_nextFreeAppearanceSlot >= cwRHIObject::kAppearanceSlotCount) {
        return kNoAppearanceSlot;
    }
    const int slot = m_nextFreeAppearanceSlot;
    if (slot >= m_appearanceSlotCapacity) {
        resizeAppearanceSlots(rhi, batch, slot + 1);
    }
    ++m_nextFreeAppearanceSlot;
    return slot;
}

void cwAppearanceSlotted::retireAppearanceResource(QRhiResource* resource)
{
    if (resource) {
        m_retiredAppearanceResources.append(resource);
    }
}

void cwAppearanceSlotted::flushRetiredAppearanceResources()
{
    qDeleteAll(m_retiredAppearanceResources);
    m_retiredAppearanceResources.clear();
}
