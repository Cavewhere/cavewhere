/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwWorldToScreenMatrix.h"

cwWorldToScreenMatrix::cwWorldToScreenMatrix(QObject *parent)
    : QObject(parent),
      m_scale(new cwScale(this))
{
    connect(m_scale, &cwScale::scaleChanged, this, [this]() {
        m_currentScale.setValue(m_scale->scale());
    });

    m_matrix.setBinding([this]() {
        constexpr double meterToMilliMeter = 1000.0;

        const double mapScale = m_currentScale.value();

        // 250 m in cave = 1 m on paper at 1:250  →  1 m in cave = 4 mm on paper.
        const double toMap = mapScale * meterToMilliMeter;

        // Convert paper-mm → screen pixels (QML units) via pixel density.
        const double toScreen = toMap * m_pixelDensity.value();

        QMatrix4x4 scaleMatrix;
        scaleMatrix.scale(toScreen);
        scaleMatrix.scale(1.0, -1.0, 1.0); // Flip Y into Qt screen space.

        return scaleMatrix;
    });
}
