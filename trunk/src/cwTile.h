#ifndef CWGLTILE_H
#define CWGLTILE_H

//Our includes
#include <cwGLObject.h>

//Qt includes
#include <QVector>
#include <QVector2D>
#include <QGLBuffer>
#include <QGLShaderProgram>

//3rd party utils
#include "Forsyth.h"

class cwTile : public cwGLObject
{
public:
    cwTile();

    virtual void initalize();
    virtual void draw();

    void setTileSize(int powerOfTwoSize);
    int tileSize() const;

    QVector<unsigned int> indexes();
    QVector<QVector2D> vertices();

    void setShaderProgram(QGLShaderProgram* shaderProgram);
    QGLShaderProgram* shaderProgram() const;

protected:
    QGLShaderProgram* Program;

    QVector<unsigned int> Indexes;
    QVector<QVector2D> Vertices;

    QGLBuffer TriangleVertexBuffer;
    QGLBuffer TriangleIndexBuffer;
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

inline void cwTile::setShaderProgram(QGLShaderProgram* shaderProgram) {
    Program = shaderProgram;
}

inline QGLShaderProgram* cwTile::shaderProgram() const {
    return Program;
}


#endif // CWGLTILE_H
