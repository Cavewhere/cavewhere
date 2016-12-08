#ifndef CWMAINENTITY_H
#define CWMAINENTITY_H

//Qt inculdes
#include <QObject>

//Our inculdes
class cwLinePlotMesh;

class cwMainEntity : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwLinePlotMesh* linePlotMesh READ linePlotMesh CONSTANT)

public:
    cwMainEntity(QObject* obj = nullptr);

    cwLinePlotMesh* linePlotMesh() const;

private:
    cwLinePlotMesh* LinePlotMesh;

};

/**
* @brief cwMainEntity::linePlot
* @return
*/
inline cwLinePlotMesh* cwMainEntity::linePlotMesh() const {
    return LinePlotMesh;
}

#endif // CWMAINENTITY_H
