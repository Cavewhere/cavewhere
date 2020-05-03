#ifndef CWIMAGECOMPRESSIONUPDATER_H
#define CWIMAGECOMPRESSIONUPDATER_H

//Qt includes
#include <QObject>
#include <QPointer>

//Our includes
class cwRegionTreeModel;
class cwNote;
class cwScrap;
#include "cwFutureManagerToken.h"

class cwImageCompressionUpdater : public QObject
{
    Q_OBJECT

    Q_PROPERTY(cwRegionTreeModel* regionTreeModel READ regionTreeModel WRITE setRegionTreeModel NOTIFY regionTreeModelChanged)

public:
    explicit cwImageCompressionUpdater(QObject *parent = nullptr);

    cwRegionTreeModel* regionTreeModel() const;
    void setRegionTreeModel(cwRegionTreeModel* regionTreeModel);

    void setFutureToken(const cwFutureManagerToken& token);
    cwFutureManagerToken futureToken() const;

signals:
    void regionTreeModelChanged();

private:
    QPointer<cwRegionTreeModel> RegionTreeModel; //!<
    cwFutureManagerToken FutureToken;

    void updateAllImages();
    bool isSetup() const;
    QString filename() const;

    void recompressNotes(QList<cwNote*> notes);
    void recompressScraps(const QList<cwScrap *> &scraps);

};

#endif // CWIMAGECOMPRESSIONUPDATER_H
