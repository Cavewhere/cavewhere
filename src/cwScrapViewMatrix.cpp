#include "cwScrapViewMatrix.h"

class cwScrapViewMatrixData : public QSharedData
{
public:
    cwScrapViewMatrix::ScrapType Type = cwScrapViewMatrix::Plan;
    QVector3D EyeDirection = QVector3D(0.0, 0.0, -1.0); //In plan, look down
    QVector3D UpDirection = QVector3D(0.0, 1.0, 0.0); //Have north be up
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

/**
* The eye direction of the matrix.
*
* For Plan: Look down the negative z. This should be (0.0, 0.0, -1.0).
*/
QVector3D cwScrapViewMatrix::eyeDirection() const {
    return data->EyeDirection;
}

/**
*
*/
void cwScrapViewMatrix::setEyeDirection(QVector3D eyeDirection) {
    data->EyeDirection = eyeDirection;
}

/**
*
*/
void cwScrapViewMatrix::setUpDirection(QVector3D upDirection) {
    data->UpDirection = upDirection;
}

/**
* The up direction of the matrix.
*
* For Plan: Look down the negative z. With North being up. This should be (0.0, 1.0, 0.0).
*/
QVector3D cwScrapViewMatrix::upDirection() const {
    return UpDirection;
}


