#ifndef CWLIDARSCAN_H
#define CWLIDARSCAN_H

#include <QObject>
#include <QQmlEngine>

class cwLidarScan : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LidarScan)


public:
    cwLidarScan();
};

#endif // CWLIDARSCAN_H
