#ifndef CWSURVEXEXPORTERCAVETASK_H
#define CWSURVEXEXPORTERCAVETASK_H

//Our includes
#include <cwSurvexExporterTask.h>
class cwSurvexExporterTripTask;
class cwCave;

// Qt includes
#include <QTextStream>

class cwSurvexExporterCaveTask : public cwSurvexExporterTask
{
    Q_OBJECT
public:
    explicit cwSurvexExporterCaveTask(QObject *parent = 0);

    void setData(const cwCave& cave);

    void writeCave(QTextStream& stream, cwCave* cave);

signals:

protected:
    virtual void runTask();

private slots:
    void UpdateProgress(int tripProgress);

private:
    cwSurvexExporterTripTask* TripExporter;
    cwCave* Cave;
    int TotalProgress;

};

#endif // CWSURVEXEXPORTERCAVETASK_H
