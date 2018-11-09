#ifndef CWCOMPASSBUFFERGENERATOR_H
#define CWCOMPASSBUFFERGENERATOR_H

//Qt includes
#include <Qt3DRender/QBufferDataGenerator>
#include <QVector3D>

//Cavewhere includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwCompassBufferGenerator : public Qt3DRender::QBufferDataGenerator
{
public:
    cwCompassBufferGenerator();

    QByteArray operator()() override;
    bool operator==(const Qt3DRender::QBufferDataGenerator& other) const override;

    QT3D_FUNCTOR(cwCompassBufferGenerator)

private:
    class CompassGeometry {
    public:
        CompassGeometry() {}
        CompassGeometry(QVector3D position, QVector3D color) :
            Position(position),
            Color(color)
        {
        }

        QVector3D Position;
        QVector3D Color;
    };

    enum Direction {
        Top,
        Bottom
    };

    void generateStarGeometry(QByteArray &trianglePoints, Direction direction);

};

#endif // CWCOMPASSBUFFERGENERATOR_H
