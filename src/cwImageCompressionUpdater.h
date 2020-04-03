#ifndef CWIMAGECOMPRESSIONUPDATER_H
#define CWIMAGECOMPRESSIONUPDATER_H

#include <QObject>

class cwImageCompressionUpdater : public QObject
{
    Q_OBJECT
public:
    explicit cwImageCompressionUpdater(QObject *parent = nullptr);

signals:

};

#endif // CWIMAGECOMPRESSIONUPDATER_H
