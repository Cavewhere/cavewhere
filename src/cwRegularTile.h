#ifndef CWGLCORNERTILE_H
#define CWGLCORNERTILE_H

//Our includes
#include "cwTile.h"

class cwRegularTile : public cwTile
{
public:
    cwRegularTile();

private:
    virtual void generate();
    void generateIndexes();
    void generateVertex();
};


#endif // CWGLCORNERTILE_H
