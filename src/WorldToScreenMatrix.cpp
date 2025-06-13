//Our includes
#include "WorldToScreenMatrix.h"

//CaveWhere includes
#include "cwScale.h"

WorldToScreenMatrix::WorldToScreenMatrix(QObject* parent)
    : QObject(parent),
    m_scale(new cwScale(this))
{
    connect(m_scale, &cwScale::scaleChanged, this, [this]() {
        m_currentScale.setValue(m_scale->scale());
    });

    m_matrix.setBinding([this]() {
        constexpr double meterToMilliMeter = 1000.0;

        const double mapScale = m_currentScale.value();

        //Map in inches

        //250m in cave = 1m on the paper at 1:250
        //1m in cave = 4mm on the paper at 1:250
        const double toMap = mapScale * meterToMilliMeter;

        //To pixels on screen (units in qml)
        //qDebug() << "Pixel density:" << m_pixelDensity.value() << "pixel/mm" << toMap;
        const double toScreen = toMap * m_pixelDensity.value();

        QMatrix4x4 scaleMatrix;
        scaleMatrix.scale(toScreen);
        scaleMatrix.scale(1.0, -1.0, 1.0); //Flip to put it into qt screenspace

        return scaleMatrix;
    });

}
