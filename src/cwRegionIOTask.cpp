//Our includes
#include "cwRegionIOTask.h"
#include "cwCavingRegion.h"

cwRegionIOTask::cwRegionIOTask(QObject* parent) :
    cwProjectIOTask(parent)
{
    Region = new cwCavingRegion(this);
}


/**
      This does a deap copy of the region, so it make a snapshot
      */
void cwRegionIOTask::setCavingRegion(const cwCavingRegion& region) {
    *Region = region;
}

