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

//Std includes
#include <cstring>
#include <type_traits>
#include <span>

class cwGeometry
{
public:
    enum class Type {
        Triangles,
        Lines,
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
        Custom
    };

    enum class AttributeFormat {
        Float,
        Vec2,
        Vec3,
        Vec4
    };

    struct VertexAttribute {
        Semantic semantic = Semantic::Custom;
        AttributeFormat format = AttributeFormat::Vec3;
        int byteOffset = 0;

        int componentCount() const {
            switch (format) {
            case AttributeFormat::Float: return 1;
            case AttributeFormat::Vec2: return 2;
            case AttributeFormat::Vec3: return 3;
            case AttributeFormat::Vec4: return 4;
            }
            return 0;
        }

        int byteSize() const {
            return componentCount() * int(sizeof(float));
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
    void resizeVertices(int vertexCount) {
        if (m_vertexStride <= 0) {
            return;
        }
        m_vertexData.resize(vertexCount * m_vertexStride);
    }

    int vertexCount() const {
        if (m_vertexStride <= 0) {
            return 0;
        }
        return m_vertexData.size() / m_vertexStride;
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

    // Traits mapping for supported element types
    template <typename T>
    struct AttributeTraits;

    template <>
    struct AttributeTraits<float> {
        static constexpr AttributeFormat format = AttributeFormat::Float;
        static constexpr int componentCount = 1;
        static void pack(const float& v, float* out) {
            *out = v;
        }
    };

    template <>
    struct AttributeTraits<QVector2D> {
        static constexpr AttributeFormat format = AttributeFormat::Vec2;
        static constexpr int componentCount = 2;
        static void pack(const QVector2D& v, float* out) {
            std::memcpy(out, reinterpret_cast<const float*>(&v), sizeof(float) * 2);
        }
    };

    template <>
    struct AttributeTraits<QVector3D> {
        static constexpr AttributeFormat format = AttributeFormat::Vec3;
        static constexpr int componentCount = 3;
        static void pack(const QVector3D& v, float* out) {
            std::memcpy(out, reinterpret_cast<const float*>(&v), sizeof(float) * 3);
        }
    };

    template <>
    struct AttributeTraits<QVector4D> {
        static constexpr AttributeFormat format = AttributeFormat::Vec4;
        static constexpr int componentCount = 4;
        static void pack(const QVector4D& v, float* out) {
            std::memcpy(out, reinterpret_cast<const float*>(&v), sizeof(float) * 4);
        }
    };

    // Bulk set: QVector<T> where T is one of the supported types above
    template <typename T>
    bool set(Semantic semantic, const QVector<T>& values) {
        static_assert(
            std::is_same<T, float>::value ||
                std::is_same<T, QVector2D>::value ||
                std::is_same<T, QVector3D>::value ||
                std::is_same<T, QVector4D>::value,
            "Unsupported attribute element type. Use float, QVector2D, QVector3D, or QVector4D."
            );

        const VertexAttribute* attribute = this->attribute(semantic);
        if (attribute == nullptr) {
            return false;
        }

        return set(attribute, values);
    }

    // Bulk set: QVector<T> where T is one of the supported types above
    template <typename T>
    bool set(const VertexAttribute* attribute, const QVector<T>& values) {
        Q_ASSERT(attribute);

        static_assert(
            std::is_same<T, float>::value ||
                std::is_same<T, QVector2D>::value ||
                std::is_same<T, QVector3D>::value ||
                std::is_same<T, QVector4D>::value,
            "Unsupported attribute element type. Use float, QVector2D, QVector3D, or QVector4D."
            );

        if (!attributeFormatMatches<T>(*attribute)) {
            return false;
        }

        const int count = values.size();
        ensureVertexCapacityForCount(count);
        resizeVertices(count);

        for (int i = 0; i < count; ++i) {
            if (!set(attribute, i, values[i])) {
                return false;
            }
        }
        return true;
    }

    // Per element set: T is one of float, QVector2D, QVector3D, QVector4D
    template <typename T>
    bool set(Semantic semantic, int vertexIndex, const T& value) {
        const VertexAttribute* attribute = this->attribute(semantic);
        if (attribute == nullptr) {
            return false;
        }
        return set(attribute, vertexIndex, value);
    }

    // Per element set: T is one of float, QVector2D, QVector3D, QVector4D
    template <typename T>
    bool set(const VertexAttribute* attribute, int vertexIndex, const T& value) {
        Q_ASSERT(attribute);

        static_assert(
            std::is_same<T, float>::value ||
                std::is_same<T, QVector2D>::value ||
                std::is_same<T, QVector3D>::value ||
                std::is_same<T, QVector4D>::value,
            "Unsupported attribute element type. Use float, QVector2D, QVector3D, or QVector4D."
            );

        Q_ASSERT(attributeFormatMatches<T>(*attribute));

        char* dst = elementPointer(vertexIndex, *attribute);
        // if (dst == nullptr) {
        //     return false;
        // }

        // Directly pack into destination memory
        AttributeTraits<T>::pack(value, reinterpret_cast<float*>(dst));
        return true;
    }

    template <typename T>
    T value(Semantic semantic, int vertexIndex) const {
        static_assert(
            std::is_same<T, float>::value ||
                std::is_same<T, QVector2D>::value ||
                std::is_same<T, QVector3D>::value ||
                std::is_same<T, QVector4D>::value,
            "Unsupported attribute element type. Use float, QVector2D, QVector3D, or QVector4D."
            );

        const VertexAttribute* attribute = this->attribute(semantic);
        if (attribute == nullptr) {
            return T{};
        }

        return value<T>(attribute, vertexIndex);
    }

    template <typename T>
    T value(const VertexAttribute* attribute, int vertexIndex) const {
        Q_ASSERT(attribute);

        static_assert(
            std::is_same<T, float>::value ||
                std::is_same<T, QVector2D>::value ||
                std::is_same<T, QVector3D>::value ||
                std::is_same<T, QVector4D>::value,
            "Unsupported attribute element type. Use float, QVector2D, QVector3D, or QVector4D."
            );

        Q_ASSERT(attributeFormatMatches<T>(*attribute));

        const char* src = elementPointer(vertexIndex, *attribute);
        Q_ASSERT(src);

        T result;
        std::memcpy(reinterpret_cast<float*>(&result), src, sizeof(float) * AttributeTraits<T>::componentCount);
        return result;
    }

    template <typename T>
    QVector<T> values(Semantic semantic) const {
        static_assert(
            std::is_same<T, float>::value ||
                std::is_same<T, QVector2D>::value ||
                std::is_same<T, QVector3D>::value ||
                std::is_same<T, QVector4D>::value,
            "Unsupported attribute element type. Use float, QVector2D, QVector3D, or QVector4D."
            );

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

        for (int i = 0; i < vertexCount(); ++i) {
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
        return m_vertexData.isEmpty() || m_indices.isEmpty();
    }

    void dump() const;

    static const char* toString(Type t);
    static const char* toString(Semantic s);
    static const char* toString(AttributeFormat f);

private:
    // In class cwGeometry (private)
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

        int offset = 0;
        for (; first != last; ++first) {
            const AttributeDesc& d = *first;
            VertexAttribute a;
            a.semantic = d.semantic;
            a.format = d.format;
            a.byteOffset = offset;
            m_attributes.push_back(a);
            offset += a.byteSize();
        }
        m_vertexStride = offset;
    }

    template <typename T>
    bool attributeFormatMatches(const VertexAttribute& a) const {
        AttributeFormat expected = AttributeTraits<T>::format;
        return a.format == expected;
    }

    template<typename This>
    auto elementPointerImp(This* thiz, int vertexIndex, const VertexAttribute& attribute) const {
        Q_ASSERT(thiz->m_vertexStride > 0);
        Q_ASSERT(vertexIndex >= 0);
        Q_ASSERT(vertexIndex < thiz->vertexCount());

        const qsizetype offset =
            static_cast<qsizetype>(vertexIndex) * thiz->m_vertexStride + attribute.byteOffset;

        Q_ASSERT(offset >= 0);
        Q_ASSERT(offset + static_cast<qsizetype>(attribute.byteSize()) <= thiz->m_vertexData.size());

        return thiz->m_vertexData.data() + offset;
    }

    char* elementPointer(int vertexIndex, const VertexAttribute& attribute) {
        return elementPointerImp(this, vertexIndex, attribute);
    }

    const char* elementPointer(int vertexIndex, const VertexAttribute& attribute) const {
        return elementPointerImp(this ,vertexIndex, attribute);
    }

    void ensureVertexCapacityForCount(int count) {
        if (m_vertexStride <= 0) {
            return;
        }
        const int required = count * m_vertexStride;
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
