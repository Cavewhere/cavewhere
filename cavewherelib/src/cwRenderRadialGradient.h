#ifndef CWRADIALGRADIENTRENDEROBJECT_H
#define CWRADIALGRADIENTRENDEROBJECT_H

#include "cwRenderObject.h"
#include <QVector2D>
#include <QColor>

class cwRenderRadialGradient : public cwRenderObject
{
    Q_OBJECT
    Q_PROPERTY(QVector2D center READ center WRITE setCenter NOTIFY centerChanged)
    Q_PROPERTY(float radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(float radiusOffset READ radiusOffset WRITE setRadiusOffset NOTIFY radiusOffsetChanged)
    Q_PROPERTY(QColor color1 READ color1 WRITE setColor1 NOTIFY color1Changed)
    Q_PROPERTY(QColor color2 READ color2 WRITE setColor2 NOTIFY color2Changed)

public:
    cwRenderRadialGradient(QObject* parent = nullptr);

    QVector2D center() const;
    void setCenter(const QVector2D& center);

    float radius() const;
    void setRadius(float radius);

    float radiusOffset() const;
    void setRadiusOffset(float radiusOffset);

    QColor color1() const;
    void setColor1(const QColor& color);

    QColor color2() const;
    void setColor2(const QColor& color);

signals:
    void centerChanged();
    void radiusChanged();
    void radiusOffsetChanged();
    void color1Changed();
    void color2Changed();

protected:
    cwRHIObject* createRHIObject() override;

private:
    QVector2D m_center;
    float m_radius;
    float m_radiusOffset; // New member variable for radius offset
    QColor m_color1;
    QColor m_color2;
};

inline QVector2D cwRenderRadialGradient::center() const
{
    return m_center;
}

inline float cwRenderRadialGradient::radius() const
{
    return m_radius;
}

inline float cwRenderRadialGradient::radiusOffset() const
{
    return m_radiusOffset;
}

inline QColor cwRenderRadialGradient::color1() const
{
    return m_color1;
}

inline QColor cwRenderRadialGradient::color2() const
{
    return m_color2;
}

#endif // CWRADIALGRADIENTRENDEROBJECT_H
