#ifndef CWWALLSIMPORTDATA_H
#define CWWALLSIMPORTDATA_H

#include "cwTreeImportData.h"

class cwWallsImportData : public cwTreeImportData
{
    Q_OBJECT
public:
    cwWallsImportData(QObject* parent = 0);
    QList<cwCave*> caves();
};

#endif // CWWALLSIMPORTDATA_H
