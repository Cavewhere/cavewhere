/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "cwEdgeTile.h"

//Qt includes
#include <QDebug>

cwEdgeTile::cwEdgeTile()
{
}

void cwEdgeTile::generate()  {
    generateVertex();
    generateIndexes();
}

void cwEdgeTile::generateIndexes() {
    QVector<unsigned int> tempIndexes;

    //Create all the geometry for normal geometry
    int numVertices = numVerticesOnADimension();
    for(int row = 0; row < numVertices - 1; row++) {
        for(int column = 1; column < numVertices - 1; column++) {
            //Triangle 1
            tempIndexes.append(indexOf(column, row));
            tempIndexes.append(indexOf(column + 1, row + 1));
            tempIndexes.append(indexOf(column, row + 1));

            //Triangle 2
            tempIndexes.append(indexOf(column, row));
            tempIndexes.append(indexOf(column + 1, row));
            tempIndexes.append(indexOf(column + 1, row + 1));
        }
    }

    unsigned int largestInt = indexOf(numVertices - 1, numVertices - 1) + 1;
    unsigned int halfIndex1 = largestInt;
    unsigned int halfIndex2 = halfIndex1 + 1;

    //Create the geometry for the first column
    for(int row = 0; row < tileSize(); row++) {

        unsigned int bottomLeft = indexOf(0, row);
        unsigned int bottomRight = indexOf(1, row);
        unsigned int topLeft = indexOf(0, row + 1);
        unsigned int topRight = indexOf(1, row + 1);

        //Triangle 1
        tempIndexes.append(bottomLeft);
        tempIndexes.append(bottomRight);
        tempIndexes.append(halfIndex2);

        //Triangle 2
        tempIndexes.append(bottomRight);
        tempIndexes.append(topRight);
        tempIndexes.append(halfIndex2);

        //Triangle 3
        tempIndexes.append(topRight);
        tempIndexes.append(topLeft);
        tempIndexes.append(halfIndex2);

        //Triangle 4
        tempIndexes.append(halfIndex1);
        tempIndexes.append(halfIndex2);
        tempIndexes.append(topLeft);

        //Triangle 5
        tempIndexes.append(bottomLeft);
        tempIndexes.append(halfIndex2);
        tempIndexes.append(halfIndex1);

        halfIndex1 += 2;
        halfIndex2 += 2;
    }

    Indexes.clear();
    Indexes.reserve(tempIndexes.size());
    Indexes.resize(tempIndexes.size());

    //Can't get optimize faces working on windows
    Indexes = tempIndexes;

//    Forsyth::OptimizeFaces(tempIndexes.data(), tempIndexes.size(),
//                           halfIndex2 - 1,
//                           Indexes.data(),
//                           24);


}

void cwEdgeTile::generateVertex() {
    Vertices.clear();

    //vertex spacing
    double spacing = 1.0 / (double)tileSize();

    int numVertexes = numVerticesOnADimension();
    int totalSize = numVertexes * numVertexes;
    Vertices.reserve(totalSize);

    //Create the regualar mesh points
    for(int y = 0; y < numVertexes; y++) {
        for(int x = 0; x < numVertexes; x++) {
            float xPos = x * spacing;
            float yPos = y * spacing;

            QVector2D vertex(xPos, yPos);
            Vertices.append(vertex);
        }
    }

    //Add the points for the half spacing
    double halfSpacing = spacing / 2.0;
    for(int row = 0; row < tileSize(); row++) {
        float yPos = row * spacing + halfSpacing;
        Vertices.append(QVector2D(0.0, yPos));
        Vertices.append(QVector2D(halfSpacing, yPos));
    }

}
