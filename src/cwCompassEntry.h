#ifndef CWCOMPASSENTRY_H
#define CWCOMPASSENTRY_H

//Qt includes
#include <Qt3DCore/QEntity>
#include <QVector3D>

class cwCompassEntry : public Qt3DCore::QEntity
{
    Q_OBJECT

public:
    cwCompassEntry(Qt3DCore::QNode* parent = nullptr);

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

};

#endif // CWCOMPASSENTRY_H
