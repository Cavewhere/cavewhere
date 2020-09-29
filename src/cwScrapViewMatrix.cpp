//Our includes
#include "cwScrapViewMatrix.h"

//Qt includes
#include <QQuaternion>

class cwScrapViewMatrixData : public QSharedData
{
public:
    cwScrapViewMatrix::ScrapType Type = cwScrapViewMatrix::Plan;
    double Azimuth = 0.0; //!<
};

cwScrapViewMatrix::cwScrapViewMatrix() : data(new cwScrapViewMatrixData)
{

}

cwScrapViewMatrix::cwScrapViewMatrix(const cwScrapViewMatrix &rhs) : data(rhs.data)
{

}

cwScrapViewMatrix &cwScrapViewMatrix::operator=(const cwScrapViewMatrix &rhs)
{
    if (this != &rhs)
        data.operator=(rhs.data);
    return *this;
}

cwScrapViewMatrix::~cwScrapViewMatrix()
{

}

cwScrapViewMatrix::ScrapType cwScrapViewMatrix::type() const
{
    return data->Type;
}

void cwScrapViewMatrix::setType(cwScrapViewMatrix::ScrapType type)
{
    data->Type = type;
}

QStringList cwScrapViewMatrix::types()
{
    return {"Plan", "Running Profile", "Project Profile"};
}

double cwScrapViewMatrix::cwScrapViewMatrix::azimuth() const {
    return data->Azimuth;
}

void cwScrapViewMatrix::cwScrapViewMatrix::setAzimuth(double azimuth) {
    if(data->Azimuth != azimuth) {
        data->Azimuth = azimuth;
    }
}

/**
 * Returns the matrix that represents the scrap view matrix
 * This matrix is usually used to help warp the scraps
 */

QMatrix4x4 cwScrapViewMatrix::matrix() const {
    QMatrix4x4 matrix;
    switch(type()) {
    case Plan:
        return QMatrix4x4(); //Idenity, looking down the z with north up
    case RunningProfile:
        return QMatrix4x4(); //Not used for running profile
    case ProjectedProfile: {
        QQuaternion rotation = QQuaternion::fromAxisAndAngle(QVector3D(0.0, 0.0, 1.0),
                                                             -azimuth())
                * QQuaternion::fromAxisAndAngle(QVector3D(1.0, 0.0, 0.0), 90.0);
        QMatrix4x4 matrix;
        matrix.rotate(rotation);
        return matrix;
    }
    }
    return QMatrix();
}

bool cwScrapViewMatrix::operator==(const cwScrapViewMatrix &other) const
{
    if(other.data == data) {
        return true;
    }

    return type() == other.type()
            && azimuth() == other.azimuth();
}
