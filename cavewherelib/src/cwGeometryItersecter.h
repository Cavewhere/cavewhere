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
class cwRenderObject;

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
        Object(cwRenderObject* parent, uint64_t id, QVector<QVector3D> points, QVector<uint> indexes, PrimitiveType type) :
            Parent(parent),
            Id(id),
            Points(points),
            Indexes(indexes),
            Type(type)
        {}

        cwRenderObject* parent() const { return Parent; }
        uint64_t id() const { return Id; }
        const QVector<QVector3D>& points() const { return Points; }
        const QVector<uint>& indexes() const { return Indexes; }
        PrimitiveType type() const { return Type; }

    private:

        cwRenderObject* Parent;
        uint64_t Id;
        QVector<QVector3D> Points;
        QVector<uint> Indexes;
        PrimitiveType Type;
    };

    cwGeometryItersecter();

    void addObject(const cwGeometryItersecter::Object& object);
    void clear(cwRenderObject* parentObject = nullptr);
    void removeObject(cwRenderObject* parentObject, uint64_t id);

    double intersects(const QRay3D& ray) const;


private:

    class Node {
    public:
        Node();
        Node(const cwGeometryItersecter::Object& object, int indexInIndexes);
        Node(const QBox3D boundingBox, const cwGeometryItersecter::Object& object);

        QBox3D BoundingBox;
        cwGeometryItersecter::Object Object;

        static QBox3D triangleToBoundingBox(const cwGeometryItersecter::Object & object, int indexInIndexes);
        static QBox3D lineToBoundingBox(const cwGeometryItersecter::Object & object, int indexInIndexes);
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
