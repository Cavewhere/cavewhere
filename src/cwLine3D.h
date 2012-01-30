#ifndef CWLINE3D_H
#define CWLINE3D_H

//Qt includes
#include <QVector3D>

class cwLine3D
{
public:
    cwLine3D();
    cwLine3D(QVector3D p0, QVector3D p1);

    void setP0(QVector3D p0);
    QVector3D p0() const;

    void setP1(QVector3D p1);
    QVector3D p1() const;

    float length() const;
    QVector3D unitVector() const;
    QVector3D vector() const;

private:
    QVector3D P0;
    QVector3D P1;
};

inline void cwLine3D::setP0(QVector3D p0) {
    P0 = p0;
}

inline QVector3D cwLine3D::p0() const {
    return P0;
}

inline void cwLine3D::setP1(QVector3D p1) {
    P1 = p1;
}

inline QVector3D cwLine3D::p1() const {
    return P1;
}

inline float cwLine3D::length() const {
    return vector().length();
}

inline QVector3D cwLine3D::unitVector() const {
    return vector().normalized();
}

inline QVector3D cwLine3D::vector() const {
    return P0 - P1;
}

#endif // CWLINE3D_H
