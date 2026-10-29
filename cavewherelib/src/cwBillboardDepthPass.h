/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWBILLBOARDDEPTHPASS_H
#define CWBILLBOARDDEPTHPASS_H

//Our includes
#include "cwBillboardDepthMode.h"
#include "CaveWhereLibExport.h"

//Qt includes
#include <QVector>
#include <rhi/qrhi.h>

namespace cw::billboard {

// One draw of a layer: its depth state and the opacity factor its instances
// draw at.
struct DepthPass {
    bool depthTest = true;
    bool depthWrite = true;
    QRhiGraphicsPipeline::CompareOp depthOp = QRhiGraphicsPipeline::Less;
    float opacity = 1.0f;

    bool operator==(const DepthPass&) const = default;
};

// The passes a layer draws in @a mode, in draw order: the hidden (x-ray) pass
// first, then the visible pass.
CAVEWHERE_LIB_EXPORT QVector<DepthPass> depthPasses(const cwBillboardDepthMode& mode);

} // namespace cw::billboard

#endif // CWBILLBOARDDEPTHPASS_H
