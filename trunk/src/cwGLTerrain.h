#ifndef CWGLTERRAIN_H
#define CWGLTERRAIN_H

//Our includes
#include <cwGLObject.h>
class cwEdgeTile;
class cwRegularTile;
class cwShaderDebugger;

//Qt includes
#include <QGLShaderProgram>
#include <QTimer>

class cwGLTerrain : public cwGLObject
{
    Q_OBJECT
public:
    explicit cwGLTerrain(QObject *parent = 0);

    virtual void initalize();
    virtual void draw();


    int numberOfLevels() const;


    int tileTessilationSize() const;

    void setTileSize(float sizeInMeters);
    float tileSize() const;

signals:
    void redraw();

public slots:
    void setNumberOfLevels(int levels);
    void setTileTessilationSize(int size);

//private slots:
//    void updateTime();

private:
    bool checkParameters();
    void generateGeometry();

    void drawCenter();
    void drawCorners(int level);
    void drawEdges(int level);

    int NumberOfLevels; //Number of clipmap levels
    int TessilationSize; //Number of quads in both height and width
    float TileSize; //In meters

    cwEdgeTile* EdgeTile;
    cwRegularTile* RegularTile;

    QGLShaderProgram* TileProgram;
    int UniformModelViewProjectionMatrix;

//    QTimer Timer;
//    float Angle;
    //int In_vVertex;

};

inline int cwGLTerrain::numberOfLevels() const {
    return NumberOfLevels;
}

inline int cwGLTerrain::tileTessilationSize() const {
    return TessilationSize;
}

inline float cwGLTerrain::tileSize() const {
    return TileSize;
}

#endif // CWGLTERRAIN_H
