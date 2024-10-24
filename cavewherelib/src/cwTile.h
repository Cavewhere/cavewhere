/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLTILE_H
#define CWGLTILE_H

//Our includes
#include "cwGLObject.h"

//Qt includes
#include <QVector>
#include <QVector2D>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>

//3rd party utils
#include "Forsyth.h"

class cwTile : public cwRenderObject
{
public:
    cwTile();

    virtual void initialize() override;
    virtual void releaseResources() override;
    virtual void draw() override;

    void setTileSize(int powerOfTwoSize);
    int tileSize() const;

    QVector<unsigned int> indexes();
    QVector<QVector2D> vertices();

    void setShaderProgram(QOpenGLShaderProgram* shaderProgram);
    QOpenGLShaderProgram* shaderProgram() const;

protected:
    QOpenGLShaderProgram* Program;

    QVector<unsigned int> Indexes;
    QVector<QVector2D> Vertices;

    QOpenGLBuffer TriangleVertexBuffer;
    QOpenGLBuffer TriangleIndexBuffer;
    int vVertex;

    virtual void generate() = 0;

    unsigned int indexOf(int column, int row) const;
    int numVerticesOnADimension() const;
private:
    int TileSize;
};

/**
  Returns the tiles
  */
inline int cwTile::tileSize() const {
    return TileSize;
}

inline QVector<unsigned int> cwTile::indexes() {
    return Indexes;
}

inline QVector<QVector2D> cwTile::vertices() {
    return Vertices;
}

inline int cwTile::numVerticesOnADimension() const {
    return tileSize() + 1;
}

/**
  Gets the index of a vertex at row and column
  */
inline unsigned int cwTile::indexOf(int column, int row) const {
    return numVerticesOnADimension() * row + column;
}

inline void cwTile::setShaderProgram(QOpenGLShaderProgram* shaderProgram) {
    Program = shaderProgram;
}

inline QOpenGLShaderProgram* cwTile::shaderProgram() const {
    return Program;
}


#endif // CWGLTILE_H
