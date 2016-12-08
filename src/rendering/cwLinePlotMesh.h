#ifndef CWLINEPLOTMESH_H
#define CWLINEPLOTMESH_H

//Qt includes
#include <QGeometryRenderer>
#include <QVector3D>
#include <QVector>

class cwLinePlotMesh : public Qt3DRender::QGeometryRenderer
{
    Q_OBJECT

public:
    cwLinePlotMesh(Qt3DCore::QNode *parent = nullptr);

    void setPoints(QVector<QVector3D> pointData);
    void setIndexes(QVector<unsigned int> indexData);

private:
    enum Attribute {
        PointAttribute,
        IndexAttribute
    };

    QVector<QVector3D> PointData;
    QVector<unsigned int> IndexData;
};

#endif // CWLINEPLOTMESH_H
