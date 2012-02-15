#ifndef CWGLOBALS_H
#define CWGLOBALS_H

//Qt includes
#include <QString>

class cwGlobals
{
public:
    cwGlobals();

    static const double PI;
    static const double RadiansToDegrees;
    static const double DegreesToRadians;

    static QString addExtension(QString filename, QString extensionHint);
};

#endif // CWGLOBALS_H
