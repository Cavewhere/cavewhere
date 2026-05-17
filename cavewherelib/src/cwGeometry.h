// #pragma once

// #include <QVector>
// #include <QVector3D>
// #include <QMatrix4x4>
// #include <QMetaType>

// struct cwGeometry
// {
//     enum PrimitiveType {
//         Triangles,
//         Lines,
//         None
//     };

//     PrimitiveType type = None;
//     QVector<QVector3D> vertices;
//     QVector<uint32_t> indices;
//     QMatrix4x4 transform;
//     bool cullBackfaces = true;

//     bool isEmpty() const {
//         return vertices.isEmpty() || indices.isEmpty();
//     }

// private:

// };


#pragma once

#include <QByteArray>
#include <QVector>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>
#include <QtGlobal>
#include <QtTypes>
#include <QtNumeric>

//Std includes
#include <array>
#include <cmath>
#include <cstring>
#include <type_traits>
#include <span>

#include "CaveWhereLibExport.h"

class CAVEWHERE_LIB_EXPORT cwGeometry
{
public:
    enum class Type {
        Triangles,
        Lines,
        Points,
        None
    };

    static const char* typeName(Type t) {
        switch (t) {
        case Type::Triangles: return "Triangles";
        case Type::Lines:     return "Lines";
        case Type::Points:    return "Points";
        case Type::None:      return "None";
        }
        Q_UNREACHABLE_RETURN("Unknown");
    }

    enum class Semantic {
        Position,
        Normal,
        Tangent,
        Bitangent,
        Color0,
        TexCoord0,
        TexCoord1,
        Classification,
        Custom
    };

    enum class AttributeFormat {
        // Float family
        Float,
        Vec2,
        Vec3,
        Vec4,
        // Unsigned 32-bit integer family
        UInt,
        UInt2,
        UInt3,
        UInt4,
        // Signed 32-bit integer family
        SInt,
        SInt2,
        SInt3,
        SInt4,
        // 16-bit float family
        Half,
        Half2,
        Half3,
        Half4,
        // Normalized byte family (shader reads float in [0,1]); Qt RHI exposes only 1/2/4
        UNormByte,
        UNormByte2,
        UNormByte4
    };

    // Storage layout for vertex data. Chosen at construction; immutable.
    //   Interleaved — single buffer, all attributes share one stride. The
    //                 traditional "vertex struct" layout. GPU-friendly for
    //                 the common case where the vertex shader reads all
    //                 attributes together.
    //   Separated   — one buffer per attribute, each tightly packed. Lets
    //                 callers (or the GPU) update / upload one attribute
    //                 independently of the others.
    enum class LayoutMode {
        Interleaved,
        Separated
    };

    // Maps 1:1 to QRhiVertexInputBinding (one entry per `setBindings`).
    struct VertexBuffer {
        const QByteArray* data; // non-null
        int stride;             // bytes between consecutive vertices in this buffer
    };

    // Maps 1:1 to QRhiVertexInputAttribute (one entry per `setAttributes`).
    struct AttributeLocation {
        int bindingIndex; // index into vertexBuffers()
        int byteOffset;   // bytes from start of a vertex in that buffer
    };

    // Bytes per component for a given format.
    // Float / UInt / SInt families: 4. Half family: 2. UNormByte family: 1.
    static int componentByteSize(AttributeFormat f) {
        switch (f) {
        case AttributeFormat::Float:
        case AttributeFormat::Vec2:
        case AttributeFormat::Vec3:
        case AttributeFormat::Vec4:
        case AttributeFormat::UInt:
        case AttributeFormat::UInt2:
        case AttributeFormat::UInt3:
        case AttributeFormat::UInt4:
        case AttributeFormat::SInt:
        case AttributeFormat::SInt2:
        case AttributeFormat::SInt3:
        case AttributeFormat::SInt4:
            return 4;
        case AttributeFormat::Half:
        case AttributeFormat::Half2:
        case AttributeFormat::Half3:
        case AttributeFormat::Half4:
            return 2;
        case AttributeFormat::UNormByte:
        case AttributeFormat::UNormByte2:
        case AttributeFormat::UNormByte4:
            return 1;
        }
        return 0;
    }

    // Quantise a float in [0,1] to [0,255] for packing into an UNormByte attribute.
    // Out-of-range inputs are clamped.
    static quint8 toUNormByte(float v) {
        const float clamped = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        return quint8(std::lround(clamped * 255.0f));
    }

    struct VertexAttribute {
        Semantic semantic = Semantic::Custom;
        AttributeFormat format = AttributeFormat::Vec3;
        // Storage descriptors baked at buildLayout time so set<T>/value<T>
        // touch no per-call branch on LayoutMode.
        // Interleaved: bufferIndex=0, bufferStride=totalStride, byteOffsetInBuffer=running offset.
        // Separated:   bufferIndex=i, bufferStride=byteSize,    byteOffsetInBuffer=0.
        int bufferIndex = 0;
        int bufferStride = 0;
        int byteOffsetInBuffer = 0;

        int componentCount() const {
            switch (format) {
            case AttributeFormat::Float:
            case AttributeFormat::UInt:
            case AttributeFormat::SInt:
            case AttributeFormat::Half:
            case AttributeFormat::UNormByte:
                return 1;
            case AttributeFormat::Vec2:
            case AttributeFormat::UInt2:
            case AttributeFormat::SInt2:
            case AttributeFormat::Half2:
            case AttributeFormat::UNormByte2:
                return 2;
            case AttributeFormat::Vec3:
            case AttributeFormat::UInt3:
            case AttributeFormat::SInt3:
            case AttributeFormat::Half3:
                return 3;
            case AttributeFormat::Vec4:
            case AttributeFormat::UInt4:
            case AttributeFormat::SInt4:
            case AttributeFormat::Half4:
            case AttributeFormat::UNormByte4:
                return 4;
            }
            return 0;
        }

        int byteSize() const {
            return componentCount() * componentByteSize(format);
        }
    };

    // New: declarative layout at construction
    struct AttributeDesc {
        Semantic semantic;
        AttributeFormat format;
    };

    cwGeometry() = default;

    cwGeometry(std::initializer_list<AttributeDesc> layout,
               LayoutMode mode = LayoutMode::Interleaved) {
        buildLayout(layout.begin(), layout.end(), mode);
    }

    cwGeometry(QVector<AttributeDesc> layout,
               LayoutMode mode = LayoutMode::Interleaved) {
        buildLayout(layout.begin(), layout.end(), mode);
    }

    cwGeometry(std::span<AttributeDesc> layout,
               LayoutMode mode = LayoutMode::Interleaved) {
        buildLayout(layout.begin(), layout.end(), mode);
    }

    LayoutMode layoutMode() const {
        return m_layoutMode;
    }

    const VertexAttribute* attribute(Semantic semantic) const;

    const QVector<VertexAttribute>& attributes() const {
        return m_attributes;
    }

    // Views onto the raw vertex storage, suitable for handing to
    // QRhiVertexInputLayout::setBindings() / setAttributes().
    //   Interleaved: vertexBuffers() returns 1 VertexBuffer; attributeLocations()
    //                returns N entries all with bindingIndex=0.
    //   Separated:   vertexBuffers() returns N VertexBuffers; attributeLocations()
    //                returns N entries with bindingIndex=i, byteOffset=0.
    QList<VertexBuffer> vertexBuffers() const {
        QList<VertexBuffer> out;
        out.reserve(int(m_vertexBuffers.size()));
        for (qsizetype i = 0; i < m_vertexBuffers.size(); ++i) {
            out.append({ &m_vertexBuffers[i], m_bufferStrides[i] });
        }
        return out;
    }

    QList<AttributeLocation> attributeLocations() const {
        QList<AttributeLocation> out;
        out.reserve(m_attributes.size());
        for (const VertexAttribute& a : m_attributes) {
            out.append({ a.bufferIndex, a.byteOffsetInBuffer });
        }
        return out;
    }

    // Direct read access to a single vertex buffer by binding index.
    // Most callers should prefer attribute(...) + value<T>(...).
    const QByteArray* vertexBuffer(int bindingIndex) const {
        Q_ASSERT(bindingIndex >= 0 && bindingIndex < m_vertexBuffers.size());
        return &m_vertexBuffers[bindingIndex];
    }

    // Power-user write accessor for bulk producers (e.g. cwLazLoader's
    // parallel chunk writer). The templated set<T> is too slow per-vertex
    // for those hot paths. Returns null if bindingIndex is out of range.
    QByteArray* mutableVertexBuffer(int bindingIndex) {
        if (bindingIndex < 0 || bindingIndex >= m_vertexBuffers.size()) {
            return nullptr;
        }
        return &m_vertexBuffers[bindingIndex];
    }

    // ----- Sizing -----
    // qsizetype: int byte-arithmetic overflows around 179M Vec3 vertices.
    // Resizes every backing buffer so all attributes have the same vertex count.
    void resizeVertices(qsizetype vertexCount) {
        for (qsizetype i = 0; i < m_vertexBuffers.size(); ++i) {
            const int stride = m_bufferStrides[i];
            if (stride <= 0) {
                continue;
            }
            m_vertexBuffers[i].resize(vertexCount * qsizetype(stride));
        }
    }

    qsizetype vertexCount() const {
        if (m_vertexBuffers.isEmpty() || m_bufferStrides[0] <= 0) {
            return 0;
        }
        return m_vertexBuffers[0].size() / qsizetype(m_bufferStrides[0]);
    }

    void clearVertexData() {
        for (QByteArray& buf : m_vertexBuffers) {
            buf.clear();
        }
    }

    void setIndices(QVector<uint32_t> indices) {
        m_indices = std::move(indices);
    }

    QVector<uint32_t>& indicesMutable() {
        return m_indices;
    }

    const QVector<uint32_t>& indices() const {
        return m_indices;
    }

    void clearIndexData() {
        m_indices.clear();
    }

    // =========================
    // Generic attribute setters
    // =========================

    // Traits mapping for supported element types. Specialisations live at the
    // bottom of this header — one per (C++ type, AttributeFormat) pairing.
    // Each specialisation derives from PackedAttributeTraits and inherits:
    //   - static constexpr AttributeFormat format
    //   - static constexpr int componentCount
    //   - static constexpr int byteSize          (bytes written by pack)
    //   - static void pack(const T&, void*)      (writes byteSize bytes)
    template <typename T>
    struct AttributeTraits;

    // Shared base for AttributeTraits specialisations — defined at the bottom
    // of this header. Component is the underlying scalar (float, quint32, …);
    // N is the component count.
    template <typename T, typename Component, int N, AttributeFormat F>
    struct PackedAttributeTraits;

    // SFINAE check for whether a specialisation of AttributeTraits<T> exists.
    // Trait specialisations all define `byteSize`; we probe for it.
    template <typename T, typename = void>
    struct is_supported_attribute_type : std::false_type {};

    template <typename T>
    struct is_supported_attribute_type<T, std::void_t<decltype(AttributeTraits<T>::byteSize)>>
        : std::true_type {};

    // Bulk set: QVector<T> where T has an AttributeTraits specialisation
    template <typename T>
    bool set(Semantic semantic, const QVector<T>& values) {
        static_assert(is_supported_attribute_type<T>::value,
                      "Unsupported attribute element type. Add a cwGeometry::AttributeTraits<T> specialisation.");

        const VertexAttribute* attribute = this->attribute(semantic);
        if (attribute == nullptr) {
            return false;
        }

        return set(attribute, values);
    }

    // Bulk set: QVector<T> where T has an AttributeTraits specialisation
    template <typename T>
    bool set(const VertexAttribute* attribute, const QVector<T>& values) {
        Q_ASSERT(attribute);

        static_assert(is_supported_attribute_type<T>::value,
                      "Unsupported attribute element type. Add a cwGeometry::AttributeTraits<T> specialisation.");

        if (!attributeFormatMatches<T>(*attribute)) {
            return false;
        }

        const qsizetype count = values.size();
        ensureVertexCapacityForCount(count);
        resizeVertices(count);

        for (qsizetype i = 0; i < count; ++i) {
            if (!set(attribute, i, values[i])) {
                return false;
            }
        }
        return true;
    }

    // Per-element set: T must have an AttributeTraits specialisation
    template <typename T>
    bool set(Semantic semantic, qsizetype vertexIndex, const T& value) {
        const VertexAttribute* attribute = this->attribute(semantic);
        if (attribute == nullptr) {
            return false;
        }
        return set(attribute, vertexIndex, value);
    }

    // Per-element set: T must have an AttributeTraits specialisation
    template <typename T>
    bool set(const VertexAttribute* attribute, qsizetype vertexIndex, const T& value) {
        Q_ASSERT(attribute);

        static_assert(is_supported_attribute_type<T>::value,
                      "Unsupported attribute element type. Add a cwGeometry::AttributeTraits<T> specialisation.");

        Q_ASSERT(attributeFormatMatches<T>(*attribute));

        char* dst = elementPointer(vertexIndex, *attribute);
        AttributeTraits<T>::pack(value, dst);
        return true;
    }

    template <typename T>
    T value(Semantic semantic, qsizetype vertexIndex) const {
        static_assert(is_supported_attribute_type<T>::value,
                      "Unsupported attribute element type. Add a cwGeometry::AttributeTraits<T> specialisation.");

        const VertexAttribute* attribute = this->attribute(semantic);
        if (attribute == nullptr) {
            return T{};
        }

        return value<T>(attribute, vertexIndex);
    }

    template <typename T>
    T value(const VertexAttribute* attribute, qsizetype vertexIndex) const {
        Q_ASSERT(attribute);

        static_assert(is_supported_attribute_type<T>::value,
                      "Unsupported attribute element type. Add a cwGeometry::AttributeTraits<T> specialisation.");

        Q_ASSERT(attributeFormatMatches<T>(*attribute));

        const char* src = elementPointer(vertexIndex, *attribute);
        Q_ASSERT(src);

        T result;
        std::memcpy(&result, src, AttributeTraits<T>::byteSize);
        return result;
    }

    template <typename T>
    QVector<T> values(Semantic semantic) const {
        static_assert(is_supported_attribute_type<T>::value,
                      "Unsupported attribute element type. Add a cwGeometry::AttributeTraits<T> specialisation.");

        const VertexAttribute* attribute = this->attribute(semantic);
        if (attribute == nullptr) {
            return {};
        }

        return values<T>(attribute);
    }

    template <typename T>
    QVector<T> values(const VertexAttribute* attribute) const {
        Q_ASSERT(attribute);

        QVector<T> result;
        result.reserve(vertexCount());

        for (qsizetype i = 0; i < vertexCount(); ++i) {
            result.push_back(value<T>(attribute, i));
        }

        return result;
    }

    // ----- Primitive utilities -----
    void appendTriangle(uint32_t i0, uint32_t i1, uint32_t i2) {
        m_indices.push_back(i0);
        m_indices.push_back(i1);
        m_indices.push_back(i2);
    }

    void appendLine(uint32_t i0, uint32_t i1) {
        m_indices.push_back(i0);
        m_indices.push_back(i1);
    }

    Type type() const {
        return m_type;
    }

    void setType(Type t) {
        m_type = t;
    }

    const QMatrix4x4& transform() const {
        return m_transform;
    }

    void setTransform(const QMatrix4x4& t) {
        m_transform = t;
    }

    bool cullBackfaces() const {
        return m_cullBackfaces;
    }

    void setCullBackfaces(bool enable) {
        m_cullBackfaces = enable;
    }

    bool isEmpty() const {
        if (vertexCount() == 0) {
            return true;
        }
        // Non-indexed primitives (Points) draw in vertex order — indices are optional.
        if (m_type == Type::Points) {
            return false;
        }
        return m_indices.isEmpty();
    }

    void dump() const;

    static const char* toString(Type t);
    static const char* toString(Semantic s);
    static const char* toString(AttributeFormat f);
    static const char* toString(LayoutMode m);

private:
    // Builds m_attributes, m_vertexBuffers, and m_bufferStrides. Each
    // VertexAttribute is stamped with bufferIndex/bufferStride/byteOffsetInBuffer
    // so the templated hot path (set<T> / value<T> / elementPointer) never
    // checks m_layoutMode at runtime.
    //
    // Interleaved: 1 buffer, all attributes share one stride; each attribute's
    //              offset is rounded up to its component byte size; the total
    //              stride is rounded up to 4 bytes — the safe vertex-buffer
    //              minimum for Vulkan / D3D / Metal bindings.
    // Separated:   N buffers, each with stride = max(byteSize, 4). Sub-4-byte
    //              attributes pay 3 bytes/vertex padding (use UNormByte4 for
    //              tight packing of 4 byte channels).
    template <class It>
    void buildLayout(It first, It last, LayoutMode mode) {
        m_layoutMode = mode;

        const auto count = std::distance(first, last);
        if (count > 0) {
            m_attributes.reserve(int(count));
        }

        constexpr int kVertexBufferAlignment = 4;

        if (mode == LayoutMode::Interleaved) {
            int offset = 0;
            for (auto it = first; it != last; ++it) {
                const AttributeDesc& d = *it;
                VertexAttribute a;
                a.semantic = d.semantic;
                a.format = d.format;
                a.bufferIndex = 0;
                const int compSize = componentByteSize(d.format);
                if (compSize > 0) {
                    offset = roundUpToAlignment(offset, compSize);
                }
                a.byteOffsetInBuffer = offset;
                m_attributes.push_back(a);
                offset += a.byteSize();
            }
            const int stride = roundUpToAlignment(offset, kVertexBufferAlignment);
            // Stamp the shared stride into every attribute after the total is known.
            for (VertexAttribute& a : m_attributes) {
                a.bufferStride = stride;
            }
            m_vertexBuffers.resize(1);
            m_bufferStrides.resize(1);
            m_bufferStrides[0] = stride;
        } else {
            // Separated: one buffer per attribute.
            int bindingIndex = 0;
            for (auto it = first; it != last; ++it) {
                const AttributeDesc& d = *it;
                VertexAttribute a;
                a.semantic = d.semantic;
                a.format = d.format;
                a.bufferIndex = bindingIndex;
                a.byteOffsetInBuffer = 0;
                // Per-buffer stride. Sub-4-byte attributes pad up to 4 (the
                // vertex-binding safe minimum) — wasteful for UNormByte but
                // avoids per-buffer alignment surprises.
                a.bufferStride = roundUpToAlignment(a.byteSize(), kVertexBufferAlignment);
                m_attributes.push_back(a);
                ++bindingIndex;
            }
            m_vertexBuffers.resize(m_attributes.size());
            m_bufferStrides.resize(m_attributes.size());
            for (int i = 0; i < m_attributes.size(); ++i) {
                m_bufferStrides[i] = m_attributes[i].bufferStride;
            }
        }
    }

    static int roundUpToAlignment(int value, int alignment) {
        if (alignment <= 1) {
            return value;
        }
        const int remainder = value % alignment;
        if (remainder == 0) {
            return value;
        }
        return value + (alignment - remainder);
    }

    template <typename T>
    bool attributeFormatMatches(const VertexAttribute& a) const {
        AttributeFormat expected = AttributeTraits<T>::format;
        return a.format == expected;
    }

    template<typename This>
    static auto elementPointerImp(This* thiz, qsizetype vertexIndex, const VertexAttribute& attribute) {
        Q_ASSERT(attribute.bufferStride > 0);
        Q_ASSERT(vertexIndex >= 0);
        Q_ASSERT(vertexIndex < thiz->vertexCount());
        Q_ASSERT(attribute.bufferIndex >= 0
                 && attribute.bufferIndex < thiz->m_vertexBuffers.size());

        const qsizetype offset =
            vertexIndex * qsizetype(attribute.bufferStride)
            + qsizetype(attribute.byteOffsetInBuffer);

        Q_ASSERT(offset >= 0);
        Q_ASSERT(offset + qsizetype(attribute.byteSize())
                 <= thiz->m_vertexBuffers[attribute.bufferIndex].size());

        return thiz->m_vertexBuffers[attribute.bufferIndex].data() + offset;
    }

    char* elementPointer(qsizetype vertexIndex, const VertexAttribute& attribute) {
        return elementPointerImp(this, vertexIndex, attribute);
    }

    const char* elementPointer(qsizetype vertexIndex, const VertexAttribute& attribute) const {
        return elementPointerImp(this, vertexIndex, attribute);
    }

    void ensureVertexCapacityForCount(qsizetype count) {
        for (qsizetype i = 0; i < m_vertexBuffers.size(); ++i) {
            const int stride = m_bufferStrides[i];
            if (stride <= 0) {
                continue;
            }
            const qsizetype required = count * qsizetype(stride);
            if (m_vertexBuffers[i].size() < required) {
                m_vertexBuffers[i].resize(required);
            }
        }
    }

    Type m_type = Type::None;
    LayoutMode m_layoutMode = LayoutMode::Interleaved;
    QVector<QByteArray> m_vertexBuffers;
    QVector<int> m_bufferStrides;
    QVector<uint32_t> m_indices;
    QVector<VertexAttribute> m_attributes;
    QMatrix4x4 m_transform;
    bool m_cullBackfaces = true;

    // private data ends here
};

// ============================================================
// AttributeTraits specialisations
//
// Every supported C++ type derives from PackedAttributeTraits, which provides
// `format`, `componentCount`, `byteSize`, and a memcpy `pack`. Component is
// the underlying scalar (float / quint32 / qfloat16 / quint8); N is the number
// of components. byteSize is always sizeof(Component) * N — independent of
// sizeof(T), which may include padding for QVector* types.
//
// To teach cwGeometry a new C++ type: add one one-line specialisation below.
// ============================================================

template <typename T, typename Component, int N, cwGeometry::AttributeFormat F>
struct cwGeometry::PackedAttributeTraits {
    static constexpr AttributeFormat format = F;
    static constexpr int componentCount = N;
    static constexpr int byteSize = sizeof(Component) * N;
    // For std::array<C, N>, &v points to the first element (guaranteed
    // contiguous storage, same address as v.data()), so this works for both
    // scalar T and array T.
    static void pack(const T& v, void* out) {
        std::memcpy(out, &v, byteSize);
    }
};

// ----- Float family -----
template <> struct cwGeometry::AttributeTraits<float>
    : cwGeometry::PackedAttributeTraits<float, float, 1, cwGeometry::AttributeFormat::Float> {};
template <> struct cwGeometry::AttributeTraits<QVector2D>
    : cwGeometry::PackedAttributeTraits<QVector2D, float, 2, cwGeometry::AttributeFormat::Vec2> {};
template <> struct cwGeometry::AttributeTraits<QVector3D>
    : cwGeometry::PackedAttributeTraits<QVector3D, float, 3, cwGeometry::AttributeFormat::Vec3> {};
template <> struct cwGeometry::AttributeTraits<QVector4D>
    : cwGeometry::PackedAttributeTraits<QVector4D, float, 4, cwGeometry::AttributeFormat::Vec4> {};

// ----- Unsigned 32-bit integer family -----
template <> struct cwGeometry::AttributeTraits<quint32>
    : cwGeometry::PackedAttributeTraits<quint32, quint32, 1, cwGeometry::AttributeFormat::UInt> {};
template <> struct cwGeometry::AttributeTraits<std::array<quint32, 2>>
    : cwGeometry::PackedAttributeTraits<std::array<quint32, 2>, quint32, 2, cwGeometry::AttributeFormat::UInt2> {};
template <> struct cwGeometry::AttributeTraits<std::array<quint32, 3>>
    : cwGeometry::PackedAttributeTraits<std::array<quint32, 3>, quint32, 3, cwGeometry::AttributeFormat::UInt3> {};
template <> struct cwGeometry::AttributeTraits<std::array<quint32, 4>>
    : cwGeometry::PackedAttributeTraits<std::array<quint32, 4>, quint32, 4, cwGeometry::AttributeFormat::UInt4> {};

// ----- Signed 32-bit integer family -----
template <> struct cwGeometry::AttributeTraits<qint32>
    : cwGeometry::PackedAttributeTraits<qint32, qint32, 1, cwGeometry::AttributeFormat::SInt> {};
template <> struct cwGeometry::AttributeTraits<std::array<qint32, 2>>
    : cwGeometry::PackedAttributeTraits<std::array<qint32, 2>, qint32, 2, cwGeometry::AttributeFormat::SInt2> {};
template <> struct cwGeometry::AttributeTraits<std::array<qint32, 3>>
    : cwGeometry::PackedAttributeTraits<std::array<qint32, 3>, qint32, 3, cwGeometry::AttributeFormat::SInt3> {};
template <> struct cwGeometry::AttributeTraits<std::array<qint32, 4>>
    : cwGeometry::PackedAttributeTraits<std::array<qint32, 4>, qint32, 4, cwGeometry::AttributeFormat::SInt4> {};

// ----- 16-bit float family -----
template <> struct cwGeometry::AttributeTraits<qfloat16>
    : cwGeometry::PackedAttributeTraits<qfloat16, qfloat16, 1, cwGeometry::AttributeFormat::Half> {};
template <> struct cwGeometry::AttributeTraits<std::array<qfloat16, 2>>
    : cwGeometry::PackedAttributeTraits<std::array<qfloat16, 2>, qfloat16, 2, cwGeometry::AttributeFormat::Half2> {};
template <> struct cwGeometry::AttributeTraits<std::array<qfloat16, 3>>
    : cwGeometry::PackedAttributeTraits<std::array<qfloat16, 3>, qfloat16, 3, cwGeometry::AttributeFormat::Half3> {};
template <> struct cwGeometry::AttributeTraits<std::array<qfloat16, 4>>
    : cwGeometry::PackedAttributeTraits<std::array<qfloat16, 4>, qfloat16, 4, cwGeometry::AttributeFormat::Half4> {};

// ----- Normalized byte family -----
// Caller pre-quantises floats to [0,255] via cwGeometry::toUNormByte().
// std::array<quint8, 3> is intentionally NOT specialised — Qt RHI exposes
// no 3-channel UNormByte format. Use UNormByte4 for 3 logical channels.
template <> struct cwGeometry::AttributeTraits<quint8>
    : cwGeometry::PackedAttributeTraits<quint8, quint8, 1, cwGeometry::AttributeFormat::UNormByte> {};
template <> struct cwGeometry::AttributeTraits<std::array<quint8, 2>>
    : cwGeometry::PackedAttributeTraits<std::array<quint8, 2>, quint8, 2, cwGeometry::AttributeFormat::UNormByte2> {};
template <> struct cwGeometry::AttributeTraits<std::array<quint8, 4>>
    : cwGeometry::PackedAttributeTraits<std::array<quint8, 4>, quint8, 4, cwGeometry::AttributeFormat::UNormByte4> {};
