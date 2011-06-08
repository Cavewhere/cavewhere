//Our includes
#include "cwRegularTile.h"

//Std includes
#include "math.h"

//Qt includes
#include <QDebug>




cwRegularTile::cwRegularTile()
{
}

void cwRegularTile::generate()  {
    generateVertex();
    generateIndexes();
}

void cwRegularTile::generateIndexes() {
    QVector<unsigned int> tempIndexes;

    int numVertices = numVerticesOnADimension();
    for(int row = 0; row < numVertices - 1; row++) {
        for(int column = 0; column < numVertices - 1; column++) {
            //Triangle 1
            tempIndexes.append(indexOf(column, row));
            tempIndexes.append(indexOf(column + 1, row + 1));
            tempIndexes.append(indexOf(column, row + 1));

            //Triangle
            tempIndexes.append(indexOf(column, row));
            tempIndexes.append(indexOf(column + 1, row));
            tempIndexes.append(indexOf(column + 1, row + 1));
        }
    }

    unsigned int largestInt = indexOf(numVertices - 1, numVertices - 1) + 1;

    Indexes.clear();
    Indexes.reserve(tempIndexes.size());
    Indexes.resize(tempIndexes.size());

    Forsyth::OptimizeFaces(tempIndexes.data(), tempIndexes.size(),
                           largestInt,
                           Indexes.data(),
                           24);

}

void cwRegularTile::generateVertex() {
    Vertices.clear();

    //vertex spacing
    double spacing = 1.0 / (double)tileSize();

    qDebug() << "Spacing: " << spacing;

    int numVertexes = numVerticesOnADimension();
    int totalSize = numVertexes * numVertexes;
    Vertices.reserve(totalSize);

    for(int y = 0; y < numVertexes; y++) {
        for(int x = 0; x < numVertexes; x++) {
            float xPos = x * spacing;
            float yPos = y * spacing;

            QVector2D vertex(xPos, yPos);
            Vertices.append(vertex);
        }
    }
}
