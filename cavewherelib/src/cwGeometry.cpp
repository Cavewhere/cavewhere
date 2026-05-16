#include "cwGeometry.h"

// cwGeometry::cwGeometry() {}

const cwGeometry::VertexAttribute *cwGeometry::attribute(Semantic semantic) const
{
    for (const VertexAttribute& attribute : m_attributes) {
        if (attribute.semantic == semantic) {
            return &attribute;
        }
    }
    return nullptr;
}



void cwGeometry::dump() const
{
    const qsizetype vtxCount   = vertexCount();
    const int attrCount        = m_attributes.size();
    const qsizetype idxCount   = m_indices.size();
    const qsizetype vtxBytes   = m_vertexData.size();

    qDebug().noquote()
        << "cwGeometry dump:"
        << "\n  Type:           " << toString(m_type)
        << "\n  Vertices:       " << vtxCount
        << "\n  Vertex stride:  " << m_vertexStride << " bytes"
        << "\n  Vertex bytes:   " << vtxBytes
        << "\n  Indices:        " << idxCount
        << "\n  Cull backfaces: " << (m_cullBackfaces ? "true" : "false")
        << "\n  Attributes:     " << attrCount;

    for (int i = 0; i < attrCount; ++i) {
        const VertexAttribute& a = m_attributes[i];
        qDebug().noquote()
            << "    [" << i << "]"
            << " semantic=" << toString(a.semantic)
            << " format="   << toString(a.format)
            << " offset="   << a.byteOffset
            << " size="     << a.byteSize() << "B";
    }
}

const char *cwGeometry::toString(Type t) {
    switch (t) {
    case Type::Triangles: return "Triangles";
    case Type::Lines:     return "Lines";
    case Type::Points:    return "Points";
    case Type::None:      return "None";
    }
    return "Unknown";
}

const char *cwGeometry::toString(Semantic s) {
    switch (s) {
    case Semantic::Position:       return "Position";
    case Semantic::Normal:         return "Normal";
    case Semantic::Tangent:        return "Tangent";
    case Semantic::Bitangent:      return "Bitangent";
    case Semantic::Color0:         return "Color0";
    case Semantic::TexCoord0:      return "TexCoord0";
    case Semantic::TexCoord1:      return "TexCoord1";
    case Semantic::Classification: return "Classification";
    case Semantic::Custom:         return "Custom";
    }
    return "Unknown";
}

const char *cwGeometry::toString(AttributeFormat f) {
    switch (f) {
    case AttributeFormat::Float:      return "Float";
    case AttributeFormat::Vec2:       return "Vec2";
    case AttributeFormat::Vec3:       return "Vec3";
    case AttributeFormat::Vec4:       return "Vec4";
    case AttributeFormat::UInt:       return "UInt";
    case AttributeFormat::UInt2:      return "UInt2";
    case AttributeFormat::UInt3:      return "UInt3";
    case AttributeFormat::UInt4:      return "UInt4";
    case AttributeFormat::SInt:       return "SInt";
    case AttributeFormat::SInt2:      return "SInt2";
    case AttributeFormat::SInt3:      return "SInt3";
    case AttributeFormat::SInt4:      return "SInt4";
    case AttributeFormat::Half:       return "Half";
    case AttributeFormat::Half2:      return "Half2";
    case AttributeFormat::Half3:      return "Half3";
    case AttributeFormat::Half4:      return "Half4";
    case AttributeFormat::UNormByte:  return "UNormByte";
    case AttributeFormat::UNormByte2: return "UNormByte2";
    case AttributeFormat::UNormByte4: return "UNormByte4";
    }
    return "Unknown";
}
