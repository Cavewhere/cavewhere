#include "cwRenderRadialGradient.h"
#include "cwRhiRadialGradient.h"

cwRenderRadialGradient::cwRenderRadialGradient(QObject* parent)
    : cwRenderObject(parent),
    m_center(0.5f, -0.7f),
    m_radius(1.3f),
    m_radiusOffset(0.7f),  // Initialize radiusOffset
    m_color1("#92D7F8"),
    m_color2("#F3F8FB")
{
}

void cwRenderRadialGradient::setCenter(const QVector2D& center)
{
    if (m_center != center) {
        m_center = center;
        emit centerChanged();
        update();
    }
}

void cwRenderRadialGradient::setRadius(float radius)
{
    if (!qFuzzyCompare(m_radius, radius)) {
        m_radius = radius;
        emit radiusChanged();
        update();
    }
}

void cwRenderRadialGradient::setRadiusOffset(float radiusOffset)
{
    if (!qFuzzyCompare(m_radiusOffset, radiusOffset)) {
        m_radiusOffset = radiusOffset;
        emit radiusOffsetChanged();
        update();
    }
}

void cwRenderRadialGradient::setColor1(const QColor& color)
{
    if (m_color1 != color) {
        m_color1 = color;
        emit color1Changed();
        update();
    }
}

void cwRenderRadialGradient::setColor2(const QColor& color)
{
    if (m_color2 != color) {
        m_color2 = color;
        emit color2Changed();
        update();
    }
}

cwRHIObject* cwRenderRadialGradient::createRHIObject()
{
    return new cwRhiRadialGradient();
}
