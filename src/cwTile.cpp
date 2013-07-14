/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwTile.h"

cwTile::cwTile() :
    TileSize(0)
{
    Program = NULL;
}

/**
  \brief Called when the opengl context is good
  */
void cwTile::initialize() {
    TriangleIndexBuffer = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    TriangleVertexBuffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);

    TriangleIndexBuffer.create();
    TriangleVertexBuffer.create();

    vVertex = Program->attributeLocation("vVertex");
}

/**
  \brief Draws the
  */
void cwTile::draw() {


    TriangleIndexBuffer.bind();
    TriangleVertexBuffer.bind();

    Program->setAttributeBuffer(vVertex, GL_FLOAT, 0, 2);
    Program->enableAttributeArray(vVertex);

    glDrawElements(GL_TRIANGLES, indexes().size(), GL_UNSIGNED_INT, NULL);

    TriangleVertexBuffer.release();
    TriangleIndexBuffer.release();
}

/**
   Sets the tile size.  If the tile size is the same, this function does nothing.

   This will also generate the geometry for the tile.

   The tile size must be divisible by two.  The number of vertics is size + 1 per
   dimension

  */
void cwTile::setTileSize(int size) {
    size = size - size % 2; //Make it even
    if(tileSize() == size) { return; }
    TileSize = size;

    //Generate in the subclasses
    generate();

    TriangleIndexBuffer.bind();
    TriangleIndexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    TriangleIndexBuffer.allocate(Indexes.data(), Indexes.size() * sizeof(GLuint));
    TriangleIndexBuffer.release();

    TriangleVertexBuffer.bind();
    TriangleVertexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    TriangleVertexBuffer.allocate(Vertices.data(), Vertices.size() * sizeof(QVector2D));
    TriangleVertexBuffer.release();
}
