/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGEOMETRYITERSECTER_H
#define CWGEOMETRYITERSECTER_H

//Qt includes
#include <QVector>
#include <QVector3D>
#include <QRay3D>
#include <QBox3D>

//Our includes
class cwGLObject;

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
        Object() : Parent(nullptr), Id(-1), Type(None) { }
        Object(cwGLObject* parent, uint id, QVector<QVector3D> points, QVector<uint> indexes, PrimitiveType type) :
            Parent(parent),
            Id(id),
            Points(points),
            Indexes(indexes),
            Type(type)
        {}

        cwGLObject* parent() const { return Parent; }
        uint id() const { return Id; }
        const QVector<QVector3D>& points() const { return Points; }
        const QVector<uint>& indexes() const { return Indexes; }
        PrimitiveType type() const { return Type; }

    private:

        cwGLObject* Parent;
        uint Id;
        QVector<QVector3D> Points;
        QVector<uint> Indexes;
        PrimitiveType Type;
    };

    cwGeometryItersecter();

    void addObject(const cwGeometryItersecter::Object& object);
    void clear(cwGLObject* parentObject = nullptr);
    void removeObject(cwGLObject* parentObject, uint id);

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
        QBox3D lineToBoundingBox() const;

    };

    QList<Node> Nodes;

    void addTriangles(const cwGeometryItersecter::Object& object);
    void addLines(const cwGeometryItersecter::Object& object);

    double nearestNeighbor(const QRay3D& ray) const;
//    double LineLineIntersect(QVector3D p1, QVector3D p2, QVector3D p3, QVector3D p4) const;
};

inline uint qHash(const cwGeometryItersecter::Object& object) {
    return object.id();
}

#endif // CWGEOMETRYITERSECTER_H
