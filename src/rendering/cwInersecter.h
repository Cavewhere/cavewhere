#ifndef CWINERSECTER_H
#define CWINERSECTER_H

//Qt3d includes
#include <Qt3DCore/QComponent>
#include <QRay3D>

class cwInersecter : public Qt3DCore::QComponent
{
    Q_OBJECT

public:
    cwInersecter(QNode *parent = nullptr);

    double intersects(const QRay3D& ray) const;


};

#endif // CWINERSECTER_H
