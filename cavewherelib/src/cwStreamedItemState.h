/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSTREAMEDITEMSTATE_H
#define CWSTREAMEDITEMSTATE_H

//Qt includes
#include <QtGlobal>

//Qt RHI
#include <rhi/qrhi.h>

//Std includes
#include <limits>
#include <optional>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwKtx2Codec.h"

/**
 * The streaming state machine for one textured item: what mip level it holds,
 * what the camera last asked for, which load is open, and the chain that has
 * landed and is being uploaded.
 *
 * Everything here is arithmetic over that state — no RHI device, no streamer, no
 * frame. The owner (cwRhiTexturedItems) turns the Action a transition returns
 * into a cwTextureStreamer call, so the transitions can be tested on their own.
 *
 * Lives on the render thread, like the item that owns it.
 */
class CAVEWHERE_LIB_EXPORT cwStreamedItemState
{
public:
    //! The resident or requested level when the item holds neither
    static constexpr int kNoLevel = -1;

    /**
     * The pinned base level is loaded ahead of every detail level: an item
     * without a texture has nothing to draw, and everything else is a refinement.
     */
    static constexpr quint64 kPinnedBasePriority = std::numeric_limits<quint64>::max();

    /**
     * A demotion outranks every refinement — it is what brings the scene back
     * under budget — but still yields to an item that has nothing to draw at all.
     */
    static constexpr quint64 kDemotionPriority = kPinnedBasePriority - 1;

    /**
     * An offscreen job is blocked until its levels land, so its refinements
     * outrank every live one — but an item with nothing to draw, and the
     * demotion that brings the scene back under budget, still go first.
     */
    static constexpr quint64 kExportPriority = kDemotionPriority - 1;

    //! Which queue a selection's refinement request joins
    enum class StreamPriority {
        Live,   //!< ranked by the live camera's screen-space-error deficit
        Export  //!< ahead of every live refinement, behind a pinned-base first load
    };

    //! Why the open load is running
    enum class RequestKind {
        Refining,  //!< selection wants more detail than the item holds
        Demoting   //!< the budget is taking detail back
    };

    /**
     * The one load an item may have open. Replaces the old "requested level is
     * the sentinel" plus "a bool says which direction" pair: an item either has
     * a request or it does not, and the request knows its own kind.
     */
    struct OpenRequest {
        int level = kNoLevel;
        RequestKind kind = RequestKind::Refining;
    };

    /**
     * What the owner should tell the streamer after a transition.
     */
    struct Action {
        enum class Kind {
            None,     //!< the streamer already has the right thing open
            Request,  //!< ask for `level` at `priority`
            Cancel    //!< drop the open load; what is resident is what is wanted
        };

        Kind kind = Kind::None;
        int level = kNoLevel;
        quint64 priority = 0;
    };

    /**
     * Levels that have landed on the render thread and are being uploaded into
     * stagingTexture, one budgeted level per frame. Nothing samples
     * stagingTexture until the last level lands, so a half-built chain can
     * straddle frames; the swap onto the item's texture is what makes it visible.
     */
    struct PendingUpload {
        cwCompressedTexture readyLevels;
        int readyTopLevel = kNoLevel;
        int nextLevelToUpload = 0;
        QRhiTexture* stagingTexture = nullptr;
    };

    cwStreamedItemState() = default;
    ~cwStreamedItemState();

    cwStreamedItemState(const cwStreamedItemState&) = delete;
    cwStreamedItemState& operator=(const cwStreamedItemState&) = delete;

    //! The most detailed level the item's texture holds
    int residentTopLevel() const { return m_residentTopLevel; }

    //! The level the camera asked for the last time the item was gathered
    int desiredTopLevel() const { return m_desiredTopLevel; }

    bool hasOpenRequest() const { return m_request.has_value(); }

    //! The level an open load is running for, or kNoLevel when nothing is open
    int requestedTopLevel() const { return m_request ? m_request->level : kNoLevel; }

    //! True while the open load gives detail back rather than adding it
    bool isDemotionInFlight() const
    {
        return m_request && m_request->kind == RequestKind::Demoting;
    }

    /**
     * The level the item is heading for: what a load is running for, or what it
     * holds when nothing is open.
     */
    int targetTopLevel() const { return m_request ? m_request->level : m_residentTopLevel; }

    //! Coarser than the camera asked for, counting an item that holds nothing yet
    bool isBelowDesired() const
    {
        return m_residentTopLevel == kNoLevel || m_residentTopLevel > m_desiredTopLevel;
    }

    /**
     * True while the item holds nothing and has nothing on the way. Its pinned
     * base is the only thing worth asking for, so selection can skip its math.
     */
    bool needsPinnedBase() const { return m_residentTopLevel == kNoLevel && !m_request; }

    //! Opens the first load for an item with nothing to draw, ahead of every refinement
    Action requestPinnedBase(int baseLevel);

    /**
     * Folds this frame's selection in: records @a desired and asks for it when
     * it is finer than what the item is heading for. Giving detail back is the
     * budget's job, in requestDemotion().
     */
    Action refineTo(int desired, StreamPriority priority);

    /**
     * Opens a budget demotion to @a newTopLevel. The coarse chain lands through
     * the same drain and swap a refinement uses, so the fine texture keeps
     * drawing until it is complete.
     */
    Action requestDemotion(int newTopLevel);

    //! True when @a topLevel is what the open load is running for
    bool wantsLevel(int topLevel) const
    {
        return m_request && m_request->level == topLevel;
    }

    //! Stages a landed chain for upload, dropping whatever half-built one was there
    void beginUpload(const cwCompressedTexture& levels, int topLevel);

    bool hasPendingUpload() const { return m_pendingUpload.readyTopLevel != kNoLevel; }
    const PendingUpload& pendingUpload() const { return m_pendingUpload; }
    PendingUpload& pendingUpload() { return m_pendingUpload; }

    /**
     * The staged chain is complete: hands its texture to the caller, makes its
     * level the resident one, and closes the request it landed for. The caller
     * owns the texture from here on.
     */
    QRhiTexture* takeUploadedTexture();

    /**
     * Closes the open request, so selection can ask for the level again instead
     * of the item stalling forever. What is resident and whatever chain an
     * earlier request already staged are untouched — the item keeps drawing
     * what it has, and a landed chain is left to finish.
     */
    void abandonRequest();

    /**
     * A staged chain that cannot be uploaded: creation failed, or the bytes are
     * corrupt. Drops the chain and closes the request it landed for, leaving a
     * newer request open.
     */
    void failUpload();

    /**
     * Forgets the request, the staged chain and what is resident, without
     * touching the caller's texture — a replacement must land before the item
     * stops drawing what it has.
     */
    void releaseAll();

    /**
     * Forgets the level the camera asked for, so the item asks again from
     * whatever camera brings the view back rather than from the one that left.
     */
    void forgetDesired() { m_desiredTopLevel = kNoLevel; }

private:
    //! Drops the staged chain and its staging texture, keeping the request
    void clearUpload();

    int m_residentTopLevel = kNoLevel;
    int m_desiredTopLevel = kNoLevel;
    std::optional<OpenRequest> m_request;
    PendingUpload m_pendingUpload;
};

#endif // CWSTREAMEDITEMSTATE_H
