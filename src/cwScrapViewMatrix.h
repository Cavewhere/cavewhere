#ifndef CWSCRAPVIEWMATRIX_H
#define CWSCRAPVIEWMATRIX_H

//Qt includes
#include <QSharedDataPointer>
#include <QObject>
#include <QVector3D>
#include <QMatrix4x4>

class cwScrapViewMatrixData;

class cwScrapViewMatrix
{
    Q_GADGET

    Q_PROPERTY(ScrapType type READ type WRITE setType)
    Q_PROPERTY(QVector3D eyeDirection READ eyeDirection WRITE setEyeDirection)
    Q_PROPERTY(QVector3D upDirection READ upDirection WRITE setUpDirection)
    Q_PROPERTY(QMatrix4x4 matrix READ matrix)

public:
    cwScrapViewMatrix();
    cwScrapViewMatrix(const cwScrapViewMatrix &);
    cwScrapViewMatrix &operator=(const cwScrapViewMatrix &);
    ~cwScrapViewMatrix();

    enum ScrapType {
        Plan,
        RunningProfile,
        Custom
    };
    Q_ENUM(ScrapType);

    ScrapType type() const;
    void setType(ScrapType type);

    Q_INVOKABLE static QStringList types();

    QVector3D eyeDirection() const;
    void setEyeDirection(QVector3D eyeDirection);

    QVector3D upDirection() const;
    void setUpDirection(QVector3D upDirection);

    QMatrix4x4 matrix() const;

private:
    QSharedDataPointer<cwScrapViewMatrixData> data;
};

/**
 * Returns the matrix that represents the scrap view matrix
 * This matrix is usually used to help warp the scraps
 */
inline QMatrix4x4 cwScrapViewMatrix::matrix() const {
    QMatrix4x4 matrix;
    matrix.lookAt(eyeDirection(), //Eye
                  QVector3D(0.0, 0.0, 0.0), //Center or origin
                  upDirection()); //Up

    return matrix;
}

#endif // CWSCRAPVIEWMATRIX_H
