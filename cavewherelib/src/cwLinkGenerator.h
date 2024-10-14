/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWLINKGENERATOR_H
#define CWLINKGENERATOR_H

//Qt includes
#include <QObject>
#include <QPointer>
#include <QQmlEngine>

//Our inculdes
#include "cwPageSelectionModel.h"
class cwCave;
class cwNote;
class cwScrap;
class cwTrip;

/**
 * @brief The cwLinkGenerator class
 *
 * This class creates links for cwPageSelectionModel. This converts an object to a page
 * that the user can view the data for that object.
 */
class cwLinkGenerator : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LinkGenerator)

    Q_PROPERTY(cwPageSelectionModel* pageSelectionModel READ pageSelectionModel WRITE setPageSelectionModel NOTIFY pageSelectionModelChanged)

public:
    explicit cwLinkGenerator(QObject *parent = 0);
    ~cwLinkGenerator();

    cwPageSelectionModel* pageSelectionModel() const;
    void setPageSelectionModel(cwPageSelectionModel* pageSelectionModel);

    Q_INVOKABLE void gotoScrap(cwScrap* scrap);
    Q_INVOKABLE void gotoNote(cwNote* note);

    Q_INVOKABLE QString caveLink(cwCave* cave);
    Q_INVOKABLE QString tripLink(cwTrip* trip);
    Q_INVOKABLE QString scrapLink(cwScrap* scrap);
    Q_INVOKABLE QString noteLink(cwNote* note);

signals:
    void pageSelectionModelChanged();

public slots:

private:
    QPointer<cwPageSelectionModel> PageSelectionModel; //!<

};

#endif // CWLINKGENERATOR_H
