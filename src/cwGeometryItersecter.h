#ifndef CWGEOMETRYITERSECTER_H
#define CWGEOMETRYITERSECTER_H

//Qt includes
#include <QVector>
#include <QVector3D>
#include <QRay3D>
#include <QBox3D>


class cwGeometryItersecter
{
public:

    enum PrimitiveType {
        Triangles,
        Lines,
        None
    };

    class Object {
    public:

        Object() : Id(0), Type(None) { }
        Object(uint id, QVector<QVector3D> points, QVector<uint> indexes, PrimitiveType type) :
            Id(id),
            Points(points),
            Indexes(indexes),
            Type(type)
        {}

        uint Id;
        QVector<QVector3D> Points;
        QVector<uint> Indexes;
        PrimitiveType Type;
    };

    cwGeometryItersecter();

    void addObject(const cwGeometryItersecter::Object& object);

    double intersects(const QRay3D& ray) const;


private:

    class Node {
    public:
        Node();
        Node(const cwGeometryItersecter::Object& object, int indexInIndexes);

        QBox3D BoundingBox;
        cwGeometryItersecter::Object Object;
        int IndexInIndexes; //Where in Object this Node point's to

    private:
        QBox3D triangleToBoundingBox() const;

    };

    QList<Node> Nodes;

    void addTriangles(const cwGeometryItersecter::Object& object);

    double nearestNeighbor(const QRay3D& ray) const;
//    double LineLineIntersect(QVector3D p1, QVector3D p2, QVector3D p3, QVector3D p4) const;
};

inline uint qHash(const cwGeometryItersecter::Object& object) {
    return object.Id;
}

#endif // CWGEOMETRYITERSECTER_H
