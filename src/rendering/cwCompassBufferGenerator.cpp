//Our includes
#include "cwCompassBufferGenerator.h"

//Qt includes
#include <QVector>
#include <QMatrix4x4>

cwCompassBufferGenerator::cwCompassBufferGenerator()
{

}

/**
 * Returns the compass buffer data. The data makes triangles that are white and black.
 */
QByteArray cwCompassBufferGenerator::operator()()
{
    QByteArray pointData;
    generateStarGeometry(pointData, Top);
    generateStarGeometry(pointData, Bottom);
    return pointData;
}

/**
 * Returns true if the other is a cwCompassBufferGeneartor and false if it's not
 *
 * All cwCompassBufferGenerators produce the same data and therefore are the same
 */
bool cwCompassBufferGenerator::operator==(const Qt3DRender::QBufferDataGenerator &other) const
{
    auto otherCompassBuffer = functor_cast<cwCompassBufferGenerator>(&other);
    return otherCompassBuffer != nullptr; //All cwCompassBufferGenerators are the same
}

void cwCompassBufferGenerator::generateStarGeometry(QByteArray &trianglePoints,
                                                    cwCompassBufferGenerator::Direction direction)
{
    QVector<QVector3D> defaultPoints; //Draw with line strip
    defaultPoints.append(QVector3D(0.0, 0.0, 0.15));
    defaultPoints.append(QVector3D(-.2, 0.2, 0.0));
    defaultPoints.append(QVector3D(0.0, 0.2, 0.15));
    defaultPoints.append(QVector3D(-.2, 0.2, 0.0));
    defaultPoints.append(QVector3D(0.0, 0.2, 0.15));
    defaultPoints.append(QVector3D(0.0, 1.0, 0.0));

    QMatrix4x4 rotationMatrix;
    rotationMatrix.setToIdentity();

    QVector3D black(0.0, 0.0, 0.0); //Black
    QVector3D white(0.9, 0.9, 0.9); //White
    QVector3D currentColor;

    //Which side should be black first
    int firstBlack;

    switch(direction) {
    case Top:
        firstBlack = 0;
        break;
    case Bottom:
        firstBlack = 1;
        break;
    }

    //The top half of the compass rose
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 2; j++) {
            currentColor = j == firstBlack ? black : white;

            for(int p = 0; p < defaultPoints.size(); p++) {
                QVector3D point;

                if(i == 0 && p == 5) {
                    //Make the north more north
                    QVector3D extendendNorth = QVector3D(0.0, 1.5, 0.0);
                    point = rotationMatrix * extendendNorth;
                } else {
                    point = rotationMatrix * defaultPoints[p];
                }

                if(direction == Bottom) {
                    //The bottom of the compass rose is flat
                    point.setZ(0.0);
                }

                CompassGeometry trianglePoint(point, currentColor);
                const char* trianglePointData = reinterpret_cast<const char*>(&trianglePoint);
                trianglePoints.append(trianglePointData, sizeof(trianglePoint));
            }

            rotationMatrix.scale(-1.0, 1.0, 1.0);
        }
        rotationMatrix.rotate(90.0, 0.0, 0.0, 1.0);
    }
}
