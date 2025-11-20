#ifndef CWRUNNINGPROFILESCRAPVIEWMATRIX_H
#define CWRUNNINGPROFILESCRAPVIEWMATRIX_H

//Our includes
#include "cwAbstractScrapViewMatrix.h"

class CAVEWHERE_LIB_EXPORT cwRunningProfileScrapViewMatrix : public cwAbstractScrapViewMatrix
{
    Q_OBJECT
public:
    cwRunningProfileScrapViewMatrix(QObject* parent = nullptr);

    class CAVEWHERE_LIB_EXPORT Data : public cwAbstractScrapViewMatrix::Data {
    public:
        Data() = default;
        Data(QVector3D from, QVector3D to) :
            Valid(true),
            From(from),
            To(to)
        {}

        QVector3D from() const { return From; }
        QVector3D to() const { return To; }

        QMatrix4x4 matrix() const;
        Data *clone() const;
        Type type() const;

    private:
        bool Valid = false;
        QVector3D From;
        QVector3D To;

    protected:
        Data(const Data& other) = default;
    };

    const cwRunningProfileScrapViewMatrix::Data *data() const {
        return d();
    }

    cwRunningProfileScrapViewMatrix *clone() const;


protected:
    cwRunningProfileScrapViewMatrix(const cwRunningProfileScrapViewMatrix& other);

    cwRunningProfileScrapViewMatrix::Data *d() const {
        return static_cast<cwRunningProfileScrapViewMatrix::Data*>(cwAbstractScrapViewMatrix::d());
    }

};

#endif // CWRUNNINGPROFILESCRAPVIEWMATRIX_H
