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
        int byteOffset = 0;

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

    cwGeometry(std::initializer_list<AttributeDesc> layout) {
        buildLayout(layout.begin(), layout.end());
    }

    cwGeometry(QVector<AttributeDesc> layout) {
        buildLayout(layout.begin(), layout.end());
    }

    cwGeometry(std::span<AttributeDesc> layout) {
        buildLayout(layout.begin(), layout.end());
    }

    const VertexAttribute* attribute(Semantic semantic) const;

    int vertexStride() const {
        return m_vertexStride;
    }

    const QVector<VertexAttribute>& attributes() const {
        return m_attributes;
    }

    // ----- Sizing -----
    // qsizetype: int byte-arithmetic overflows around 179M Vec3 vertices.
    void resizeVertices(qsizetype vertexCount) {
        if (m_vertexStride <= 0) {
            return;
        }
        m_vertexData.resize(vertexCount * qsizetype(m_vertexStride));
    }

    qsizetype vertexCount() const {
        if (m_vertexStride <= 0) {
            return 0;
        }
        return m_vertexData.size() / qsizetype(m_vertexStride);
    }

    void clearVertexData() {
        m_vertexData.clear();
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

    const QByteArray& vertexData() const {
        return m_vertexData;
    }

    QByteArray& vertexDataMutable() {
        return m_vertexData;
    }

    bool isEmpty() const {
        if (m_vertexData.isEmpty()) {
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

private:
    // In class cwGeometry (private)
    // Alignment-aware: each attribute's byteOffset is rounded up to its
    // component byte size (so a Vec3 lands on a 4-byte boundary, a Half2 on
    // a 2-byte boundary). The final stride is rounded up to 4 bytes — the
    // safe vertex-buffer minimum for Vulkan / D3D / Metal bindings.
    template <class It>
    void buildLayout(It first, It last) {
        // m_attributes.clear();
        // m_vertexData.clear();
        // m_indices.clear();
        // m_vertexStride = 0;

        const auto count = std::distance(first, last);
        if (count > 0) {
            m_attributes.reserve(int(count));
        }

        constexpr int kVertexBufferAlignment = 4;

        int offset = 0;
        for (; first != last; ++first) {
            const AttributeDesc& d = *first;
            VertexAttribute a;
            a.semantic = d.semantic;
            a.format = d.format;
            const int compSize = componentByteSize(d.format);
            if (compSize > 0) {
                offset = roundUpToAlignment(offset, compSize);
            }
            a.byteOffset = offset;
            m_attributes.push_back(a);
            offset += a.byteSize();
        }
        m_vertexStride = roundUpToAlignment(offset, kVertexBufferAlignment);
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
    auto elementPointerImp(This* thiz, qsizetype vertexIndex, const VertexAttribute& attribute) const {
        Q_ASSERT(thiz->m_vertexStride > 0);
        Q_ASSERT(vertexIndex >= 0);
        Q_ASSERT(vertexIndex < thiz->vertexCount());

        const qsizetype offset =
            vertexIndex * qsizetype(thiz->m_vertexStride) + qsizetype(attribute.byteOffset);

        Q_ASSERT(offset >= 0);
        Q_ASSERT(offset + qsizetype(attribute.byteSize()) <= thiz->m_vertexData.size());

        return thiz->m_vertexData.data() + offset;
    }

    char* elementPointer(qsizetype vertexIndex, const VertexAttribute& attribute) {
        return elementPointerImp(this, vertexIndex, attribute);
    }

    const char* elementPointer(qsizetype vertexIndex, const VertexAttribute& attribute) const {
        return elementPointerImp(this, vertexIndex, attribute);
    }

    void ensureVertexCapacityForCount(qsizetype count) {
        if (m_vertexStride <= 0) {
            return;
        }
        const qsizetype required = count * qsizetype(m_vertexStride);
        if (m_vertexData.size() < required) {
            m_vertexData.resize(required);
        }
    }

    Type m_type = Type::None;
    QByteArray m_vertexData;
    QVector<uint32_t> m_indices;
    QVector<VertexAttribute> m_attributes;
    int m_vertexStride = 0;
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
