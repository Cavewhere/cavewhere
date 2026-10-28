/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLabelDeclutter.h"
#include "cwBillboardOverlayItem.h"
#include "cwCamera.h"

//Std includes
#include <algorithm>

cwLabelDeclutter::cwLabelDeclutter(QObject* parent) :
    QObject(parent)
{
}

void cwLabelDeclutter::addParticipant(cwBillboardOverlayItem* overlay)
{
    if(overlay == nullptr) {
        return;
    }
    removeParticipant(overlay);

    const cwLabelDeclutter::Tier tier = overlay->declutterTier();
    const auto position = std::upper_bound(m_participants.begin(), m_participants.end(), tier,
                                           [](cwLabelDeclutter::Tier value, const QPointer<cwBillboardOverlayItem>& participant) {
                                               return participant.isNull() || value < participant->declutterTier();
                                           });
    m_participants.insert(position, overlay);
    m_dirty = true;
}

void cwLabelDeclutter::removeParticipant(cwBillboardOverlayItem* overlay)
{
    std::erase_if(m_participants, [overlay](const QPointer<cwBillboardOverlayItem>& participant) {
        return participant.isNull() || participant == overlay;
    });
    m_dirty = true;
}

void cwLabelDeclutter::ensurePass(cwCamera* camera)
{
    if(camera == nullptr || m_inPass) {
        return;
    }

    const QMatrix4x4 matrix = camera->qtViewportMatrix() * camera->viewProjectionMatrix();
    const QRect viewport = camera->viewport();
    if(!m_dirty && m_lastCamera == camera && m_lastMatrix == matrix && m_lastViewport == viewport) {
        return;
    }

    // Cleared before the claims, so a request made during them still runs again.
    m_dirty = false;
    m_lastCamera = camera;
    m_lastMatrix = matrix;
    m_lastViewport = viewport;
    m_inPass = true;

    QSize cellSize(cwCollisionRectGrid::kMinCellSize, cwCollisionRectGrid::kMinCellSize);
    for(const QPointer<cwBillboardOverlayItem>& participant : m_participants) {
        if(!participant.isNull() && participant->camera() == camera) {
            cellSize = cellSize.expandedTo(participant->declutterCellSize());
        }
    }
    m_grid.reset(viewport, cellSize);

    // A claim can neither add nor remove participants, so the list is stable here.
    for(const QPointer<cwBillboardOverlayItem>& participant : m_participants) {
        if(!participant.isNull() && participant->camera() == camera) {
            participant->claimDeclutterSpace(m_grid);
        }
    }

    m_inPass = false;
}

void cwLabelDeclutter::invalidate()
{
    m_dirty = true;
}

void cwLabelDeclutter::schedulePass()
{
    m_dirty = true;
    if(m_passPending) {
        return;
    }
    m_passPending = true;
    QMetaObject::invokeMethod(this, &cwLabelDeclutter::runScheduledPass, Qt::QueuedConnection);
}

void cwLabelDeclutter::runScheduledPass()
{
    m_passPending = false;
    for(const QPointer<cwBillboardOverlayItem>& participant : m_participants) {
        if(!participant.isNull() && participant->camera() != nullptr) {
            ensurePass(participant->camera());
            return;
        }
    }
}
