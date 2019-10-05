#ifndef CWLINEPLOTMESH_H
#define CWLINEPLOTMESH_H

//Qt includes
#include <QGeometryRenderer>
#include <QVector3D>
#include <QVector>

class cwLinePlotMesh : public Qt3DRender::QGeometryRenderer
{
    Q_OBJECT

    Q_PROPERTY(QVector<QVector3D> points READ points WRITE setPoints NOTIFY pointsChanged)
    Q_PROPERTY(QVector<unsigned int> indexes READ indexes WRITE setIndexes NOTIFY indexesChanged)

public:
    cwLinePlotMesh(Qt3DCore::QNode *parent = nullptr);

    QVector<QVector3D> points() const;
    void setPoints(QVector<QVector3D> pointData);

    QVector<unsigned int> indexes() const;
    void setIndexes(QVector<unsigned int> indexes);

signals:
    void pointsChanged();
    void indexesChanged();

private:
    enum Attribute {
        PointAttribute,
        IndexAttribute
    };

    QVector<QVector3D> PointData;
    QVector<unsigned int> IndexData;
};

inline QVector<QVector3D> cwLinePlotMesh::points() const {
    return PointData;
}

inline QVector<unsigned int> cwLinePlotMesh::indexes() const {
    return IndexData;
}

#endif // CWLINEPLOTMESH_H
