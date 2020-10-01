#ifndef CWSCRAPVIEWMATRIX_H
#define CWSCRAPVIEWMATRIX_H

//Qt includes
#include <QSharedDataPointer>
#include <QObject>
#include <QVector3D>
#include <QMatrix4x4>

//Our includes
#include <cwGlobals.h>

class cwScrapViewMatrixData;

class CAVEWHERE_LIB_EXPORT cwScrapViewMatrix
{
    Q_GADGET

    Q_PROPERTY(ScrapType type READ type WRITE setType)
    Q_PROPERTY(double azimuth READ azimuth WRITE setAzimuth)

    Q_PROPERTY(QMatrix4x4 matrix READ matrix)

public:
    cwScrapViewMatrix();
    cwScrapViewMatrix(const cwScrapViewMatrix &);
    cwScrapViewMatrix &operator=(const cwScrapViewMatrix &);
    ~cwScrapViewMatrix();

    enum ScrapType {
        Plan,
        RunningProfile,
        ProjectedProfile
    };
    Q_ENUM(ScrapType);

    ScrapType type() const;
    void setType(ScrapType type);

    Q_INVOKABLE static QStringList types();

    double azimuth() const;
    void setAzimuth(double azimuth);

    QMatrix4x4 matrix() const;

    bool operator==(const cwScrapViewMatrix& other) const;
    bool operator!=(const cwScrapViewMatrix& other) const;

private:
    QSharedDataPointer<cwScrapViewMatrixData> data;
};

Q_DECLARE_METATYPE(cwScrapViewMatrix)

inline bool cwScrapViewMatrix::operator!=(const cwScrapViewMatrix &other) const
{
    return !operator==(other);
}


#endif // CWSCRAPVIEWMATRIX_H
