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

public:
    cwProjectedProfileScrapViewMatrix(QObject* parent = nullptr);

    double azimuth() const;
    void setAzimuth(double azimuth);

    class Data : public cwAbstractScrapViewMatrix::Data {
    public:
        Data(double azimuth) :
            Azimuth(azimuth)
        {}

        Data() = default;

        void setAzimuth(double azimuth) { Azimuth = azimuth; }
        double azimuth() const { return Azimuth; }

        // Data interface
        QMatrix4x4 matrix() const;
        Data *clone() const;

    protected:
        Data(const Data& other) = default;

    private:
        double Azimuth = 0.0; //!<

    };

    const cwProjectedProfileScrapViewMatrix::Data *data() const
    {
        return d();
    }

    cwProjectedProfileScrapViewMatrix *clone() const;

signals:
    void azimuthChanged();

protected:
    cwProjectedProfileScrapViewMatrix::Data *d() const
    {
        return static_cast<cwProjectedProfileScrapViewMatrix::Data*>(cwAbstractScrapViewMatrix::d());
    }

    cwProjectedProfileScrapViewMatrix(const cwProjectedProfileScrapViewMatrix& other);

};


#endif // CWPROJECTEDPROFILESCRAPVIEWMATRIX_H
