#ifndef CWMAINENTITY_H
#define CWMAINENTITY_H

//Qt inculdes
#include <QObject>
#include <QPointer>

//Our inculdes
class cwLinePlotMesh;

class cwMainEntity : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwLinePlotMesh* linePlotMesh READ linePlotMesh CONSTANT)

public:
    cwMainEntity(QObject* obj = nullptr);
    ~cwMainEntity();

    cwLinePlotMesh* linePlotMesh() const;

private:
    QPointer<cwLinePlotMesh> LinePlotMesh;

};



#endif // CWMAINENTITY_H
