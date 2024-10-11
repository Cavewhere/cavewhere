#ifndef CWPROJECTEDPROFILESCRAPVIEWMATRIX_H
#define CWPROJECTEDPROFILESCRAPVIEWMATRIX_H

//Qt includes
#include <QObject>

//Our includes
#include "cwAbstractScrapViewMatrix.h"

class CAVEWHERE_LIB_EXPORT cwProjectedProfileScrapViewMatrix : public cwAbstractScrapViewMatrix
{
    Q_OBJECT

    Q_PROPERTY(double azimuth READ azimuth WRITE setAzimuth NOTIFY azimuthChanged)
    Q_PROPERTY(AzimuthDirection direction READ direction WRITE setDirection NOTIFY directionChanged)
    Q_PROPERTY(QStringList directionTypes READ directionTypes CONSTANT)

    QStringList directionTypes() const;



public:
    enum AzimuthDirection {
        LookingAt,
        LeftToRight,
        RightToLeft
    };

    class CAVEWHERE_LIB_EXPORT Data : public cwAbstractScrapViewMatrix::Data {
    public:
        Data(double azimuth) :
            Azimuth(azimuth)
        {}

        Data() = default;

        void setAzimuth(double azimuth) { Azimuth = azimuth; }
        double azimuth() const { return Azimuth; }

        cwProjectedProfileScrapViewMatrix::AzimuthDirection direction() const { return Direction; }
        void setDirection(cwProjectedProfileScrapViewMatrix::AzimuthDirection direction) { Direction = direction; }

        double absoluteAzimuth() const;

        // Data interface
        QMatrix4x4 matrix() const;
        Data *clone() const;
        cwScrap::ScrapType type() const;

    protected:
        Data(const Data& other) = default;

    private:
        double Azimuth = 0.0; //!<
        cwProjectedProfileScrapViewMatrix::AzimuthDirection Direction = LookingAt; //!<
    };

    Q_ENUM(AzimuthDirection);

    cwProjectedProfileScrapViewMatrix(QObject* parent = nullptr);

    double azimuth() const;
    void setAzimuth(double azimuth);

    AzimuthDirection direction() const;
    void setDirection(AzimuthDirection direction);

    const cwProjectedProfileScrapViewMatrix::Data *data() const
    {
        return d();
    }

    cwProjectedProfileScrapViewMatrix *clone() const;

signals:
    void azimuthChanged();
    void directionChanged();

protected:
    cwProjectedProfileScrapViewMatrix::Data *d() const
    {
        return static_cast<cwProjectedProfileScrapViewMatrix::Data*>(cwAbstractScrapViewMatrix::d());
    }

    cwProjectedProfileScrapViewMatrix(const cwProjectedProfileScrapViewMatrix& other);

};


#endif // CWPROJECTEDPROFILESCRAPVIEWMATRIX_H
