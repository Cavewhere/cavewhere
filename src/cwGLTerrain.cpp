/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our include
#include "cwGLTerrain.h"
#include "cwEdgeTile.h"
#include "cwRegularTile.h"
#include "cwGLShader.h"
#include "cwShaderDebugger.h"
#include "cwCamera.h"
#include "cwGlobalDirectory.h"
#include "cwMath.h"

cwGLTerrain::cwGLTerrain(QObject *parent) :
    cwGLObject(parent)
{
    EdgeTile = new cwEdgeTile();
    RegularTile = new cwRegularTile();

    TessilationSize = 2;
    NumberOfLevels = 0;
}

cwGLTerrain::~cwGLTerrain()
{
    //FIXME: EdgeTile should be destroyed by the QSceneGraph.  Don't destroy them here
//    delete EdgeTile;
//    delete RegularTile;
}

/**
  \brief Called when the opengl context is good
  */
void cwGLTerrain::initialize() {

    cwGLShader* tileVertexShader = new cwGLShader(QOpenGLShader::Vertex);
    tileVertexShader->setSourceFile(cwGlobalDirectory::baseDirectory() + "shaders/tileVertex.vert");

    cwGLShader* tileFragmentShader = new cwGLShader(QOpenGLShader::Fragment);
    tileFragmentShader->setSourceFile(cwGlobalDirectory::baseDirectory() + "shaders/tileVertex.frag");

//    cwGLShader* tileGeometryShader = new cwGLShader(QOpenGLShader::Geometry);
//    tileGeometryShader->setSourceFile(cwGlobalDirectory::baseDirectory() + "shaders/tileVertex.geom");

    TileProgram = new QOpenGLShaderProgram();
    TileProgram->addShader(tileVertexShader);
    TileProgram->addShader(tileFragmentShader);
//    TileProgram->addShader(tileGeometryShader);

//    TileProgram->setGeometryInputType(GL_TRIANGLES);
//    TileProgram->setGeometryOutputType(GL_TRIANGLE_STRIP);

    bool linkingErrors = TileProgram->link();
    if(!linkingErrors) {
        qDebug() << "Linking errors:" << TileProgram->log();
    }

    shaderDebugger()->addShaderProgram(TileProgram);
    UniformModelViewProjectionMatrix = TileProgram->uniformLocation("ModelViewProjectionMatrix");
    UniformModelMatrix = TileProgram->uniformLocation("ModelMatrix");
    TileProgram->setUniformValue("colorBG", Qt::gray);

//    EdgeTile->setCamera(camera());
//    RegularTile->setCamera(camera());

    EdgeTile->setShaderProgram(TileProgram);
    RegularTile->setShaderProgram(TileProgram);

//    EdgeTile->setShaderDebugger(shaderDebugger());
//    RegularTile->setShaderDebugger(shaderDebugger());

    EdgeTile->initialize();
    RegularTile->initialize();

    EdgeTile->setTileSize(TessilationSize);
    RegularTile->setTileSize(TessilationSize);
}

/**
  \brief Draws the terrain
  */
void cwGLTerrain::draw() {

    if(!TileProgram->isLinked()) {
        TileProgram->release();
        return;
    }

    TileProgram->bind();

    //Draw the center tile
    drawCenter();

    //Draw the corner tiles
    for(int level = 1; level <= NumberOfLevels; level++) {
        drawCorners(level);
        drawEdges(level);
    }

    TileProgram->release();
}

/**
  Sets the number of clip map level for the terrain

  This will generate the geometry for the terrain if the
  other parameters, tessilation and tile size are good.
  */
void cwGLTerrain::setNumberOfLevels(int levels) {
    if(levels != NumberOfLevels) {
        NumberOfLevels = levels;
    }
}

/**
  Sets how many quads will make up a tile
  */
void cwGLTerrain::setTileTessilationSize(int size) {
    if(TessilationSize != size) {
        TessilationSize = size;

        EdgeTile->setTileSize(TessilationSize);
        RegularTile->setTileSize(TessilationSize);
    }
}

/**
  Sets how big a tile is in meters
  */
void cwGLTerrain::setTileSize(float sizeInMeters) {
    if(TileSize != sizeInMeters) {
        TileSize = sizeInMeters;
    }
}

/**
  Checks the NumberOfLevels, TessilationSize and TilesSize

  If these parameters are less then zero then this return false, other wise true
  */
bool cwGLTerrain::checkParameters() {
    return NumberOfLevels > 0 && TessilationSize > 0 && TileSize > 0;
}

/**
  Generates the tiles base on the terrain tile parameters
  */
void cwGLTerrain::generateGeometry() {
    if(!checkParameters()) { return; }


}

/**
  \brief Draws the center of the terrain

  The tile shader needs to be bound
  */
void cwGLTerrain::drawCenter() {
    //Draw the inter cube
    for(int row = 0; row < 4; row++) {
        float y = row - 2;

        for(int column = 0; column < 4; column++) {
            float x = column - 2;

            QMatrix4x4 modelMatrix;
            modelMatrix.translate(x, y, 0.0);

            QMatrix4x4 modelViewProjection = camera()->viewProjectionMatrix() * modelMatrix;

            TileProgram->setUniformValue(UniformModelViewProjectionMatrix, modelViewProjection);
            TileProgram->setUniformValue(UniformModelMatrix, modelMatrix);
            RegularTile->draw();
        }
    }
}

/**
  \brief Draws the corner at level
  */
void cwGLTerrain::drawCorners(int level) {

    float scale = exp2(level);

    for(int row = -2; row < 4; row += 3) {
        float y = row * scale;

        for(int column = -2; column < 4; column += 3) {
            float x = column * scale;

            QMatrix4x4 modelMatrix;
            modelMatrix.translate(x, y, 0);
            modelMatrix.scale(scale, scale, 1.0);

            QMatrix4x4 modelViewProjection = camera()->viewProjectionMatrix() * modelMatrix;

            TileProgram->setUniformValue(UniformModelViewProjectionMatrix, modelViewProjection);
            TileProgram->setUniformValue(UniformModelMatrix, modelMatrix);
            RegularTile->draw();
        }
    }
}

/**
  \brief Draw the edges at level
  */
void cwGLTerrain::drawEdges(int level) {

    float scale = exp2(level);
    float halfScale = scale / 2.0;

    //Top and bottom rows
    for(int row = -2; row < 4; row += 3) {
        float y = row * scale;
        for(int column = -1; column < 1; column++) {
            float x = column * scale;

            //Translate the quad to the correct position
            QMatrix4x4 modelMatrix;
            modelMatrix.translate(x, y, 0.0);

            //Rotate the center of the tile
            modelMatrix.translate(halfScale, halfScale, 0.0);
            if(row == -2) {
                modelMatrix.rotate(-90, 0.0, 0.0, 1.0);
            } else {
                modelMatrix.rotate(90, 0.0, 0.0, 1.0);
            }
            modelMatrix.translate(-halfScale, -halfScale, 0.0);

            //Scale the quad
            modelMatrix.scale(scale, scale, 1.0);

            QMatrix4x4 modelViewProjection = camera()->viewProjectionMatrix() * modelMatrix;

            TileProgram->setUniformValue(UniformModelViewProjectionMatrix, modelViewProjection);
            TileProgram->setUniformValue(UniformModelMatrix, modelMatrix);
            EdgeTile->draw();
        }
    }

    //Left and right rows
    for(int row = -1; row < 1; row++) {
        float y = row * scale;
        for(int column = -2; column < 4; column += 3) {
            float x = column * scale;

            QMatrix4x4 modelMatrix;
            modelMatrix.translate(x, y, 0.0);


            //Rotate the center of the tile
            modelMatrix.translate(halfScale, halfScale, 0.0);
            if(column == -2) {
                modelMatrix.rotate(180, 0.0, 0.0, 1.0);
            }
            modelMatrix.translate(-halfScale, -halfScale, 0.0);

            modelMatrix.scale(scale, scale, 1.0);

            QMatrix4x4 modelViewProjection = camera()->viewProjectionMatrix() * modelMatrix;

            TileProgram->setUniformValue(UniformModelViewProjectionMatrix, modelViewProjection);
            TileProgram->setUniformValue(UniformModelMatrix, modelMatrix);
            EdgeTile->draw();
        }
    }

}
