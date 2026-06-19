/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWAPPEARANCESLOTTED_H
#define CWAPPEARANCESLOTTED_H

#include <QList>

class QRhi;
class QRhiResource;
class QRhiResourceUpdateBatch;
class cwAppearanceOverride;

// Reusable mixin giving a cwRHIObject a per-object pool of "appearance slots":
// aligned regions of the object's per-object uniform buffer addressed at draw
// time by a dynamic offset, so an offscreen job can render the object with an
// overridden appearance without disturbing the live view. Slot 0 is the live
// appearance and always exists; override slots are handed out transiently for
// the one frame an offscreen batch is recorded in, then reclaimed.
//
// Why per-frame, not per-future (see plans/APPEARANCE_OVERRIDE_ONJOB_PLAN.html
// §8.1): an override slot's only reader is the draw that consumes its UBO offset,
// recorded in the frame the offscreen batch is built. QRhi versions Dynamic
// buffers per frame-in-flight, so once that frame's command buffer is recorded
// the slot's bytes are free to reuse next frame. The offscreen renderer therefore
// resets the override slots each frame (resetFrameAppearanceSlots) on the render
// thread and never threads an RAII handle through the asynchronous read-back. The
// allocated capacity (the UBO's size) only ever grows, to the concurrent-batch
// high-water mark; growth recreates the buffer off the per-frame path
// (resizeAppearanceSlots) and must defer-delete the prior resources so draws
// already recorded this frame keep valid pointers.
class cwAppearanceSlotted
{
public:
    // The live appearance always lives in slot 0.
    static constexpr int kLiveAppearanceSlot = 0;
    // Returned by acquireAppearanceSlot when the per-object appearance budget is
    // exhausted; the caller renders the object live instead of overriding it.
    static constexpr int kNoAppearanceSlot = -1;

    // Hand out the next free override slot for the current frame, growing the
    // per-object UBO (resizeAppearanceSlots) if the pool is full. @a batch carries
    // the slot-0 re-upload a growth performs. Render-thread only. Returns a slot
    // index >= 1, or kNoAppearanceSlot if the appearance budget is exhausted.
    int acquireAppearanceSlot(QRhi* rhi, QRhiResourceUpdateBatch* batch);

    // Free every override slot handed out this frame (the live slot 0 is kept).
    // Called once per frame by the offscreen renderer before it records the batch;
    // the prior frame's draws are already recorded, so their slots' bytes are safe
    // to reuse.
    void resetFrameAppearanceSlots() { m_nextFreeAppearanceSlot = kLiveAppearanceSlot + 1; }

    // Grow the pool so it can hold at least @a slotCount slots before any draw is
    // recorded this frame — the §8.1 pre-grow: one resize per batch instead of one
    // per overriding tile. No-op if already large enough. @a batch carries the
    // slot-0 re-upload.
    void reserveAppearanceSlots(QRhi* rhi, QRhiResourceUpdateBatch* batch, int slotCount);

    // Current allocated slot count (>= 1 once the UBO exists, 0 before).
    int appearanceSlotCapacity() const { return m_appearanceSlotCapacity; }

    // Write @a override into @a slot of this object's appearance UBO — one slot,
    // just-in-time, on @a batch. Each object interprets the opaque payload into its
    // own per-object uniform layout.
    virtual void uploadAppearance(QRhiResourceUpdateBatch* batch, int slot,
                                  const cwAppearanceOverride& override) = 0;

    // Free the GPU resources orphaned by a pool growth on a PRIOR frame (now that
    // that frame is submitted, the draws that held their pointers are done). The
    // offscreen renderer calls this once per frame, before any new growth — the
    // reclaim can't be tied to the subclass's updateResources(), which a static
    // object never runs. Render-thread only.
    void flushRetiredAppearanceResources();

protected:
    virtual ~cwAppearanceSlotted();

    // (Re)create the per-object appearance UBO (and dependent SRB) sized for
    // @a slotCount slots, carrying the live slot 0 forward (re-uploaded on @a batch).
    // The subclass must retireAppearanceResource() the prior buffer/SRB rather than
    // freeing them in place, so draws already recorded this frame keep valid
    // pointers (§8.1). The override sets m_appearanceSlotCapacity.
    virtual void resizeAppearanceSlots(QRhi* rhi, QRhiResourceUpdateBatch* batch,
                                       int slotCount) = 0;

    // Hand a buffer/SRB orphaned by a growth to the base for deferred deletion
    // (freed by the next flushRetiredAppearanceResources or at destruction). Null is
    // ignored. The base owns the lifetime so every slotted object reclaims uniformly.
    void retireAppearanceResource(QRhiResource* resource);

    // Set by the subclass's resizeAppearanceSlots once it has (re)allocated.
    int m_appearanceSlotCapacity = 0;

private:
    int m_nextFreeAppearanceSlot = kLiveAppearanceSlot + 1;

    // Buffers/SRBs orphaned by an in-frame pool growth, awaiting deferred deletion.
    QList<QRhiResource*> m_retiredAppearanceResources;
};

#endif // CWAPPEARANCESLOTTED_H
