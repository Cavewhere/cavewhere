/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "CaptureLayerInspector.h"
#include "cwCaptureCenterline.h"
#include "cwCaptureLeads.h"

// Qt includes
#include <QGraphicsItem>

namespace {
constexpr int kNoPreview = -1;

template<typename ItemType>
QVariantMap countPreviewItems(const cwCaptureViewport* viewport)
{
    const QString countKey = QStringLiteral("count");
    const QString visibleCountKey = QStringLiteral("visibleCount");

    if(viewport == nullptr || viewport->previewItem() == nullptr) {
        return {{countKey, kNoPreview}, {visibleCountKey, kNoPreview}};
    }

    int count = 0;
    int visibleCount = 0;
    const QList<QGraphicsItem*> children = viewport->previewItem()->childItems();
    for(const QGraphicsItem* child : children) {
        if(dynamic_cast<const ItemType*>(child) != nullptr) {
            ++count;
            if(child->isVisible()) {
                ++visibleCount;
            }
        }
    }
    return {{countKey, count}, {visibleCountKey, visibleCount}};
}
}

CaptureLayerInspector::CaptureLayerInspector(QObject* parent) :
    QObject(parent)
{
}

QVariantMap CaptureLayerInspector::previewLeads(cwCaptureViewport* viewport) const
{
    return countPreviewItems<cwCaptureLeads>(viewport);
}

QVariantMap CaptureLayerInspector::previewCenterlines(cwCaptureViewport* viewport) const
{
    return countPreviewItems<cwCaptureCenterline>(viewport);
}

bool CaptureLayerInspector::previewPlaced(cwCaptureViewport* viewport) const
{
    if(viewport == nullptr || viewport->previewItem() == nullptr) {
        return false;
    }

    const QList<QGraphicsItem*> children = viewport->previewItem()->childItems();
    for(const QGraphicsItem* child : children) {
        if(dynamic_cast<const cwCaptureCenterline*>(child) != nullptr && child->isVisible()) {
            return true;
        }
    }
    return false;
}
