#ifndef CWITEMTEXTURE_H
#define CWITEMTEXTURE_H

// Qt includes
#include <QImage>

// Std includes
#include <variant>

// Our includes
#include "cwStreamedTexture.h"

/**
 * A texture in exactly one representation: empty, a QImage of pixels, or a
 * cwStreamedTexture descriptor the render thread streams mip levels from.
 *
 * The either/or is structural — holding one representation drops the other —
 * so no producer, command queue, or render-thread sync has to clear a sibling
 * field to keep the invariant. A null image or descriptor is empty.
 */
class cwItemTexture
{
public:
    cwItemTexture() = default;

    //Converting on purpose: a producer assigns the representation it has and
    //the value carries which one that is
    cwItemTexture(const QImage& image)
    {
        if(!image.isNull()) {
            m_texture = image;
        }
    }

    cwItemTexture(const cwStreamedTexture& streamed)
    {
        if(!streamed.isNull()) {
            m_texture = streamed;
        }
    }

    bool isEmpty() const { return std::holds_alternative<std::monostate>(m_texture); }
    bool isImage() const { return std::holds_alternative<QImage>(m_texture); }
    bool isStreamed() const { return std::holds_alternative<cwStreamedTexture>(m_texture); }

    /**
     * The pixels, or a null QImage when this holds a descriptor or nothing
     */
    QImage image() const
    {
        const QImage* image = std::get_if<QImage>(&m_texture);
        return image != nullptr ? *image : QImage();
    }

    /**
     * The descriptor, or a null cwStreamedTexture when this holds pixels or
     * nothing
     */
    cwStreamedTexture streamed() const
    {
        const cwStreamedTexture* streamed = std::get_if<cwStreamedTexture>(&m_texture);
        return streamed != nullptr ? *streamed : cwStreamedTexture();
    }

private:
    std::variant<std::monostate, QImage, cwStreamedTexture> m_texture;
};

#endif // CWITEMTEXTURE_H
