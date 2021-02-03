//Our includes
#include "cwMainEntity.h"
#include "cwLinePlotMesh.h"

cwMainEntity::cwMainEntity(QObject* obj)
    : QObject(obj),
      LinePlotMesh(new cwLinePlotMesh(nullptr))
{

}

cwMainEntity::~cwMainEntity()
{
    if(!LinePlotMesh.isNull() && LinePlotMesh->parentNode() == nullptr) {
        //Hasn't been assigned to an entity yet
        delete LinePlotMesh;
    }
}

/**
* @brief cwMainEntity::linePlot
* @return
*/
cwLinePlotMesh* cwMainEntity::linePlotMesh() const {
    return LinePlotMesh;
}
