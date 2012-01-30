#ifndef CWXMLPROJECTSAVETASK_H
#define CWXMLPROJECTSAVETASK_H

//Our includes
#include <cwRegionIOTask.h>

class cwRegionSaveTask : public cwRegionIOTask
{
    Q_OBJECT
public:
    explicit cwRegionSaveTask(QObject *parent = 0);

signals:

public slots:

protected:
    void runTask();

private:
    void writeXMLToDatabase(QString xml);
};

#endif // CWXMLPROJECTSAVETASK_H
