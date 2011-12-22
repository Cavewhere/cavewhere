#ifndef CWGLSCRAPS_H
#define CWGLSCRAPS_H

//Our includes
#include "cwGLObject.h"
#include "cwTriangulatedData.h"
class cwCavingRegion;

//Qt includes
#include <QGLShaderProgram>
#include <QGLBuffer>

class cwGLScraps : public cwGLObject
{
    Q_OBJECT
public:
    explicit cwGLScraps(QObject *parent = 0);

    void updateGeometry();
    void setCavingRegion(cwCavingRegion* region);

    void initialize();
    void draw();
    
signals:
    
public slots:

private:
    class GLScrap {

    public:
        GLScrap();
        GLScrap(cwTriangulatedData data);

        QGLBuffer PointBuffer;
        QGLBuffer IndexBuffer;

        int NumberOfIndices;
    };

    cwCavingRegion* Region;

    QGLShaderProgram* Program;
    int UniformModelViewProjectionMatrix;
    int vVertex;

    QList<GLScrap> Scraps;

    void initializeShaders();

};

inline void cwGLScraps::setCavingRegion(cwCavingRegion *region) {
    Region = region;
}



#endif // CWGLSCRAPS_H
