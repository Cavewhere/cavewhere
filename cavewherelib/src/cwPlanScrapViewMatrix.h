#ifndef CWPLANSCRAPVIEWMATRIX_H
#define CWPLANSCRAPVIEWMATRIX_H

//Our includes
#include "cwAbstractScrapViewMatrix.h"

//Qt includes
#include <QObject>
#include <QQmlEngine>

class CAVEWHERE_LIB_EXPORT cwPlanScrapViewMatrix : public cwAbstractScrapViewMatrix
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PlanScrapViewMatrix)

public:
    class CAVEWHERE_LIB_EXPORT Data : public cwAbstractScrapViewMatrix::Data {

    public:
        Data() = default;
        QMatrix4x4 matrix() const;
        Data *clone() const;
        cwScrap::ScrapType type() const;
    };

    cwPlanScrapViewMatrix(QObject* parent = nullptr);

    // cwAbstractScrapViewMatrix interface
public:
    const cwPlanScrapViewMatrix::Data *data() const;

    cwPlanScrapViewMatrix* clone() const {
        return new cwPlanScrapViewMatrix();
    }
};

#endif // CWPLANSCRAPVIEWMATRIX_H
