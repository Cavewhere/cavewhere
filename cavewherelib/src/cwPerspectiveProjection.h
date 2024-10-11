/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPERSPECTIVEPROJECTION_H
#define CWPERSPECTIVEPROJECTION_H

//Our includes
#include "cwAbstractProjection.h"

class cwPerspectiveProjection : public cwAbstractProjection
{
    Q_OBJECT

    Q_PROPERTY(double fieldOfView READ fieldOfView WRITE setFieldOfView NOTIFY fieldOfViewChanged)

public:
    explicit cwPerspectiveProjection(QObject *parent = 0);

    double fieldOfView() const;
    void setFieldOfView(double fieldOfView);

signals:
    void fieldOfViewChanged();

public slots:

protected:
    cwProjection calculateProjection();

private:
    double FieldOfView;
    
};

/**
 * @brief cwPerspectiveProjection::fieldOfView
 * @return The field of view in degrees
 */
inline double cwPerspectiveProjection::fieldOfView() const {
    return FieldOfView;
}

#endif // CWPERSPECTIVEPROJECTION_H
