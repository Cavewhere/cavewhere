#ifndef CWPLANE_H
#define CWPLANE_H

//Qt includes
#include <QVector3D>

//Our includes
#include <cwLine3D.h>

class cwPlane
{
public:
    cwPlane();
    cwPlane(QVector3D p0, QVector3D p1, QVector3D p2);

    void setP0(QVector3D p0);
    QVector3D p0() const;

    void setP1(QVector3D p1);
    QVector3D p1() const;

    void setP2(QVector3D p2);
    QVector3D p2() const;

    QVector3D normal() const;

    QVector3D intersectAsRay(cwLine3D line, bool* hasIntersection) const;

private:
    QVector3D P0;
    QVector3D P1;
    QVector3D P2;

};

inline void cwPlane::setP0(QVector3D p0) {
    P0 = p0;
}

inline QVector3D cwPlane::p0() const {
    return P0;
}

inline void cwPlane::setP1(QVector3D p1) {
    P1 = p1;
}

inline QVector3D cwPlane::p1() const {
    return P1;
}

inline void cwPlane::setP2(QVector3D p2) {
    P2 = p2;
}

inline QVector3D cwPlane::p2() const {
    return P2;
}

inline QVector3D cwPlane::normal() const {
    return QVector3D::crossProduct(P1 - P0, P2 - P0).normalized();
}


#endif // CWPLANE_H
