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
    const int vtxCount   = vertexCount();
    const int attrCount  = m_attributes.size();
    const int idxCount   = m_indices.size();
    const int vtxBytes   = m_vertexData.size();

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
    case Type::None:      return "None";
    }
    return "Unknown";
}

const char *cwGeometry::toString(Semantic s) {
    switch (s) {
    case Semantic::Position: return "Position";
    case Semantic::Normal:   return "Normal";
    case Semantic::Tangent:  return "Tangent";
    case Semantic::Bitangent:return "Bitangent";
    case Semantic::Color0:   return "Color0";
    case Semantic::TexCoord0:return "TexCoord0";
    case Semantic::TexCoord1:return "TexCoord1";
    case Semantic::Custom:   return "Custom";
    }
    return "Unknown";
}

const char *cwGeometry::toString(AttributeFormat f) {
    switch (f) {
    case AttributeFormat::Float: return "Float";
    case AttributeFormat::Vec2:  return "Vec2";
    case AttributeFormat::Vec3:  return "Vec3";
    case AttributeFormat::Vec4:  return "Vec4";
    }
    return "Unknown";
}
