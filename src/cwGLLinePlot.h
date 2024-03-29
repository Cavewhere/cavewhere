/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLLINEPLOT_H
#define CWGLLINEPLOT_H

//Our includes
#include "cwGLObject.h"

//Qt includes
#include <QVector3D>
#include <QVector>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>

class cwGLLinePlot : public cwGLObject
{
    Q_OBJECT
public:
    explicit cwGLLinePlot(QObject *parent = 0);

    virtual void initialize();
    void releaseResources();
    virtual void draw();

    void setPoints(QVector<QVector3D> pointData);
    void setIndexes(QVector<unsigned int> indexData);

    void updateData();

signals:

public slots:


private:
    void initializeShaders();
    void initializeBuffers();

    float MaxZValue;
    float MinZValue;

    QOpenGLBuffer LinePlotVertexBuffer;
    QOpenGLBuffer LinePlotIndexBuffer;
    int IndexBufferSize;

    int vVertex; //attribute location
    int UniformModelViewProjectionMatrix; //in shader uniform location
    int UniformMaxZValue; //in shader uniform location
    int UniformMinZValue; //in shader uniform location

    QVector<QVector3D> Points;
    QVector<unsigned int> Indexes;

    QOpenGLShaderProgram* ShaderProgram;
};

#endif // CWGLLINEPLOT_H
