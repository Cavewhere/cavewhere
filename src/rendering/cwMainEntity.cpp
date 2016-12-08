//Our includes
#include "cwMainEntity.h"
#include "cwLinePlotMesh.h"

cwMainEntity::cwMainEntity(QObject* obj)
    : QObject(obj),
      LinePlotMesh(new cwLinePlotMesh(nullptr))
{

}
