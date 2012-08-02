#ifndef CWTRIANGULATE_H
#define CWTRIANGULATE_H

//Qt includes
#include <QVector>
#include <QPointF>

class cwTriangulate
{
public:
    // triangulate a contour/polygon, places results in STL vector
    // as series of triangles.
    static bool Process(const QVector<QPointF> &contour,
                        QVector<QPointF> &result);

    // compute area of a contour/polygon
    static double Area(const QVector<QPointF> &contour);

    // decide if point Px/Py is inside triangle defined by
    // (Ax,Ay) (Bx,By) (Cx,Cy)
    static bool InsideTriangle(double Ax, double Ay,
                        double Bx, double By,
                        double Cx, double Cy,
                        double Px, double Py);


  private:
    static bool Snip(const QVector<QPointF> &contour,int u,int v,int w,int n,int *V);
};

#endif // CWTRIANGULATE_H
