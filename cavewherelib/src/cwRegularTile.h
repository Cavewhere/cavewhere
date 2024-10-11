/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

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
