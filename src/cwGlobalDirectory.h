/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLOBALDIRECTORY_H
#define CWGLOBALDIRECTORY_H

//Qt includes
#include <QString>

class cwGlobalDirectory
{
public:
    cwGlobalDirectory();

    static void setupBaseDirectory();
    static QString baseDirectory();

private:
    static QString BaseDirectory;
};

inline QString cwGlobalDirectory::baseDirectory() {
    return BaseDirectory + "/";
}

#endif // CWGLOBALDIRECTORY_H
