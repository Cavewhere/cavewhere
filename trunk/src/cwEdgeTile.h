#ifndef CWEDGETILE_H
#define CWEDGETILE_H

//Our includes
#include <cwTile.h>

class cwEdgeTile : public cwTile {
public:
    cwEdgeTile();

protected:
    virtual void generate();

private:
    void generateIndexes();
    void generateVertex();

};

#endif // CWEDGETILE_H
