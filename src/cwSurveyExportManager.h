/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEYEXPORTMANGER_H
#define CWSURVEYEXPORTMANGER_H

//Qt includes
#include <QObject>
#include <QPointer>
class QThread;

//Our includes
class cwCave;
class cwTrip;
class cwCavingRegion;

/**
    This class will open dialogs to export data.

    This class controls the different survey data exporters: survex and compass
  */
class cwSurveyExportManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString currentCaveName READ currentCaveName NOTIFY updateMenu)
    Q_PROPERTY(QString currentTripName READ currentTripName NOTIFY updateMenu)

    Q_PROPERTY(cwCavingRegion* cavingRegion READ cavingRegion WRITE setCavingRegion NOTIFY cavingRegionChanged)
    Q_PROPERTY(cwCave* cave READ cave WRITE setCave NOTIFY caveChanged)
    Q_PROPERTY(cwTrip* trip READ trip WRITE setTrip NOTIFY tripChanged)

public:
    explicit cwSurveyExportManager(QObject *parent = 0);
    ~cwSurveyExportManager();

    QString currentCaveName() const;
    QString currentTripName() const;

    cwCavingRegion* cavingRegion() const;
    void setCavingRegion(cwCavingRegion* cavingRegion);

    cwCave* cave() const;
    void setCave(cwCave* cave);

    cwTrip* trip() const;
    void setTrip(cwTrip* trip);

signals:
    void updateMenu();
    void tripChanged();
    void caveChanged();
    void cavingRegionChanged();

public slots:
    void exportSurvexTrip(QString filename);
    void exportSurvexCave(QString filename);
    void exportSurvexRegion(QString filename);
    void exportCaveToCompass(QString filename);
    void exportCaveToChipdata(QString filename);

private slots:
    void exporterFinished();

    void updateActions();

private:
    //The data that will be exported
    QPointer<cwTrip> Trip; //!<
    QPointer<cwCave> Cave; //!<
    QPointer<cwCavingRegion> CavingRegion; //!<

    //The thread that the exporting runs on
    QThread* ExportThread;

    void updateCaveActions(const QModelIndex& index);
    void updateTripActions(const QModelIndex& index);

    cwCave* currentCave() const;
    cwTrip* currentTrip() const;

};

#endif // CWSURVEYEXPORTMANGER_H
