/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#pragma once

#include <rhi/qrhi.h>

#include "cwGeometry.h"

/**
 * Maps cwGeometry::AttributeFormat to QRhiVertexInputAttribute::Format.
 *
 * Pure function — no state. Used by RHI consumers (cwRHIPointCloud,
 * cwRHILinePlot, cwRhiTexturedItems) to build QRhiVertexInputLayout
 * dynamically from a geometry's attribute list.
 *
 * Coverage matches Qt 6.11 RHI exactly: Qt RHI exposes no UByte (raw 1-byte
 * integer) and no UNormByte3 — neither is exposed here. UNormByte values
 * are read as floats in [0,1] in the shader.
 */
inline QRhiVertexInputAttribute::Format toRhiFormat(cwGeometry::AttributeFormat f)
{
    using F = cwGeometry::AttributeFormat;
    switch (f) {
    case F::Float:      return QRhiVertexInputAttribute::Float;
    case F::Vec2:       return QRhiVertexInputAttribute::Float2;
    case F::Vec3:       return QRhiVertexInputAttribute::Float3;
    case F::Vec4:       return QRhiVertexInputAttribute::Float4;
    case F::UInt:       return QRhiVertexInputAttribute::UInt;
    case F::UInt2:      return QRhiVertexInputAttribute::UInt2;
    case F::UInt3:      return QRhiVertexInputAttribute::UInt3;
    case F::UInt4:      return QRhiVertexInputAttribute::UInt4;
    case F::SInt:       return QRhiVertexInputAttribute::SInt;
    case F::SInt2:      return QRhiVertexInputAttribute::SInt2;
    case F::SInt3:      return QRhiVertexInputAttribute::SInt3;
    case F::SInt4:      return QRhiVertexInputAttribute::SInt4;
    case F::Half:       return QRhiVertexInputAttribute::Half;
    case F::Half2:      return QRhiVertexInputAttribute::Half2;
    case F::Half3:      return QRhiVertexInputAttribute::Half3;
    case F::Half4:      return QRhiVertexInputAttribute::Half4;
    case F::UNormByte:  return QRhiVertexInputAttribute::UNormByte;
    case F::UNormByte2: return QRhiVertexInputAttribute::UNormByte2;
    case F::UNormByte4: return QRhiVertexInputAttribute::UNormByte4;
    }
    Q_UNREACHABLE_RETURN(QRhiVertexInputAttribute::Float);
}

/**
 * Builds a QRhiVertexInputLayout from a cwGeometry, ready to feed to
 * QRhiGraphicsPipeline::setVertexInputLayout().
 *
 * Interleaved geometry produces one binding (the shared stride) and N
 * attributes all pointing into binding 0. Separated geometry produces N
 * bindings (one per attribute, tight stride) and N attributes each
 * pointing into a distinct binding.
 *
 * The `location` of each attribute is its index in geometry.attributes()
 * — callers must match this with the shader's `layout(location = i)`
 * declarations.
 */
inline QRhiVertexInputLayout buildRhiInputLayout(const cwGeometry& geometry)
{
    const auto buffers   = geometry.vertexBuffers();
    const auto locations = geometry.attributeLocations();
    const auto& attrs    = geometry.attributes();

    QList<QRhiVertexInputBinding> bindings;
    bindings.reserve(buffers.size());
    for (const cwGeometry::VertexBuffer& vb : buffers) {
        bindings.append(QRhiVertexInputBinding(quint32(vb.stride)));
    }

    QList<QRhiVertexInputAttribute> rhiAttrs;
    rhiAttrs.reserve(attrs.size());
    for (int i = 0; i < attrs.size(); ++i) {
        rhiAttrs.append(QRhiVertexInputAttribute(
            quint32(locations[i].bindingIndex),
            quint32(i),
            toRhiFormat(attrs[i].format),
            quint32(locations[i].byteOffset)));
    }

    QRhiVertexInputLayout layout;
    layout.setBindings(bindings.cbegin(), bindings.cend());
    layout.setAttributes(rhiAttrs.cbegin(), rhiAttrs.cend());
    return layout;
}
