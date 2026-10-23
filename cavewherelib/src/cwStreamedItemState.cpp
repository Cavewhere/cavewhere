/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwStreamedItemState.h"

cwStreamedItemState::~cwStreamedItemState()
{
    delete m_pendingUpload.stagingTexture;
}

cwStreamedItemState::Action cwStreamedItemState::requestPinnedBase(int baseLevel)
{
    m_request = OpenRequest{baseLevel, RequestKind::Refining};
    return {Action::Kind::Request, baseLevel, kPinnedBasePriority};
}

cwStreamedItemState::Action cwStreamedItemState::refineTo(int desired, StreamPriority priority)
{
    m_desiredTopLevel = desired;

    const int target = targetTopLevel();
    if(desired >= target) {
        return {};
    }

    if(desired == m_residentTopLevel) {
        //The open request is a demotion the camera has changed its mind about,
        //and what is resident is already the wanted level: drop the request
        //rather than reloading bytes the item holds. The staged chain, if any,
        //belongs to a request that has already landed, so it is left to finish.
        m_request.reset();
        return {Action::Kind::Cancel};
    }

    //A demotion in flight is superseded by this: the streamer bumps the item's
    //generation, so the coarse chain is dropped when it lands.
    m_request = OpenRequest{desired, RequestKind::Refining};
    return {Action::Kind::Request, desired,
            priority == StreamPriority::Export ? kExportPriority
                                               : quint64(target - desired)};
}

cwStreamedItemState::Action cwStreamedItemState::requestDemotion(int newTopLevel)
{
    m_request = OpenRequest{newTopLevel, RequestKind::Demoting};
    return {Action::Kind::Request, newTopLevel, kDemotionPriority};
}

void cwStreamedItemState::beginUpload(const cwCompressedTexture& levels, int topLevel)
{
    clearUpload();
    m_pendingUpload.readyLevels = levels;
    m_pendingUpload.readyTopLevel = topLevel;
    m_pendingUpload.nextLevelToUpload = 0;
}

QRhiTexture* cwStreamedItemState::takeUploadedTexture()
{
    QRhiTexture* uploaded = m_pendingUpload.stagingTexture;
    const int landedTopLevel = m_pendingUpload.readyTopLevel;
    m_residentTopLevel = landedTopLevel;

    m_pendingUpload = {};   //ownership of the texture moves to the caller

    //A chain straddles frames, so selection or the budget may have opened a
    //newer request while this one was uploading. Only the request this chain
    //belongs to is closed; a newer one stays open so its result is accepted.
    if(m_request && m_request->level == landedTopLevel) {
        m_request.reset();
    }

    return uploaded;
}

void cwStreamedItemState::abandonRequest()
{
    m_request.reset();
}

void cwStreamedItemState::failUpload()
{
    const int stagedTopLevel = m_pendingUpload.readyTopLevel;
    clearUpload();

    //Same rule as takeUploadedTexture(): a request opened after this chain
    //landed is left open, so its own result is still wanted when it arrives.
    if(m_request && m_request->level == stagedTopLevel) {
        m_request.reset();
    }
}

void cwStreamedItemState::releaseAll()
{
    clearUpload();
    m_request.reset();
    m_residentTopLevel = kNoLevel;
}

void cwStreamedItemState::clearUpload()
{
    delete m_pendingUpload.stagingTexture;
    m_pendingUpload = {};
}
