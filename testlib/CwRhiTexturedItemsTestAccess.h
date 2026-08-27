/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRHITEXTUREDITEMSTESTACCESS_H
#define CWRHITEXTUREDITEMSTESTACCESS_H

//Our includes
#include "cwRhiTexturedItems.h"

// Friend accessor (declared `friend struct CwRhiTexturedItemsTestAccess` in
// cwRhiTexturedItems.h) for the per-item streaming state synchronize() derives
// on the render thread. Nothing in production reads it from outside the class,
// so it stays private rather than growing accessors the app would never call.
// Sync needs no QRhi, so a test drives it through CwRhiSceneTestAccess and reads
// the result here.
struct CwRhiTexturedItemsTestAccess {
    static bool hasItem(const cwRhiTexturedItems& items, uint32_t id) {
        return items.m_items.value(id, nullptr) != nullptr;
    }

    static double uvPerMeter(const cwRhiTexturedItems& items, uint32_t id) {
        auto* found = items.m_items.value(id, nullptr);
        return found ? found->uvPerMeter : 0.0;
    }

    static bool boundsValid(const cwRhiTexturedItems& items, uint32_t id) {
        auto* found = items.m_items.value(id, nullptr);
        return found && found->boundsValid;
    }

    static cwStreamedTexture streamSource(const cwRhiTexturedItems& items, uint32_t id) {
        auto* found = items.m_items.value(id, nullptr);
        return found ? found->streamSource : cwStreamedTexture();
    }

    static int residentTopLevel(const cwRhiTexturedItems& items, uint32_t id) {
        auto* found = items.m_items.value(id, nullptr);
        return found ? found->residentTopLevel : cwRhiTexturedItems::kNoResidentLevel;
    }

    static int requestedTopLevel(const cwRhiTexturedItems& items, uint32_t id) {
        auto* found = items.m_items.value(id, nullptr);
        return found ? found->requestedTopLevel : cwRhiTexturedItems::kNoResidentLevel;
    }

    static int desiredTopLevel(const cwRhiTexturedItems& items, uint32_t id) {
        auto* found = items.m_items.value(id, nullptr);
        return found ? found->desiredTopLevel : cwRhiTexturedItems::kNoResidentLevel;
    }

    static bool textureNeedsUpdate(const cwRhiTexturedItems& items, uint32_t id) {
        auto* found = items.m_items.value(id, nullptr);
        return found && found->textureNeedsUpdate;
    }

    // Pretends a load for @a topLevel completed, so a test can watch residency
    // survive a repeated descriptor and reset on a changed one without a GPU.
    static void setResidentTopLevel(cwRhiTexturedItems& items, uint32_t id, int topLevel) {
        if (auto* found = items.m_items.value(id, nullptr)) {
            found->residentTopLevel = topLevel;
        }
    }

    static bool demotionInFlight(const cwRhiTexturedItems& items, uint32_t id) {
        auto* found = items.m_items.value(id, nullptr);
        return found && found->demotionInFlight;
    }

    // Pretends the item was last gathered in @a frame, so a test can order a
    // fleet for the eviction planner without running frames.
    static void setLastVisibleFrame(cwRhiTexturedItems& items, uint32_t id, quint64 frame) {
        if (auto* found = items.m_items.value(id, nullptr)) {
            found->lastVisibleFrame = frame;
        }
    }

    static QRhiTexture::Format streamTargetFormat() {
        return cwRhiTexturedItems::streamTargetFormat();
    }

    static constexpr int noResidentLevel() { return cwRhiTexturedItems::kNoResidentLevel; }

    // Runs one item's selection pass without a gather — selection reads the
    // camera and budgets out of @a context and never touches the command buffer,
    // so a test can drive it with hand-built render data.
    static void selectStreamLevel(cwRhiTexturedItems& items,
                                  uint32_t id,
                                  const cwRHIObject::GatherContext& context) {
        if (auto* found = items.m_items.value(id, nullptr)) {
            items.selectStreamLevel(id, found, context);
        }
    }

    static bool hasStreamingWork(const cwRhiTexturedItems& items) {
        return items.m_streamer.hasWork();
    }
};

#endif // CWRHITEXTUREDITEMSTESTACCESS_H
