/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWREGIONIOTASK_H
#define CWREGIONIOTASK_H

//Our includes
#include "cwProjectIOTask.h"
#include "cwGlobals.h"
#include "cwCavingRegionData.h"
class cwCavingRegion;

class CAVEWHERE_LIB_EXPORT cwRegionIOTask : public cwProjectIOTask
{
    Q_OBJECT
public:
    cwRegionIOTask(QObject* parent = nullptr);
    ~cwRegionIOTask();

    void setCavingRegion(const cwCavingRegionData& region);

    void copyRegionTo(cwCavingRegion *region);

    static int protoVersion();
    static QString toVersion(int protoVersion);

    // Save/load format-version gate, shared by every loader that stamps a
    // CavewhereCommonProto.FileVersion (projects and symbology palettes).
    // Returns true when this build can fully read and write `fileVersion`. A
    // file newer than protoVersion() is not supported: re-saving it would drop
    // data the newer build added, so the loader keeps it read-only and warns.
    static bool isVersionSupported(int fileVersion);

    // The standard "created by a newer version" warning for an unsupported file.
    // `subject` names the thing ("\"cave.cwproj\"", "Symbology palette \"reef\"");
    // `consequence` is the closing clause (what is disabled plus the upgrade
    // ask). Kept here so the wording lives in one place across loaders.
    static QString newerVersionWarning(const QString &subject, int fileVersion,
                                       const QString &consequence);

protected:
    cwCavingRegionData Region;

private:

};

#endif // CWREGIONIOTASK_H
