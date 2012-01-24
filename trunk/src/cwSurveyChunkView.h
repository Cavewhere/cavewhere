#ifndef CWSURVEYCHUCKVIEW_H
#define CWSURVEYCHUCKVIEW_H

//Qt includes
#include <QObject>
#include <QDeclarativeItem>
#include <QList>
#include <QVector>
#include <QDebug>
class QValidator;

//Our includes
#include "cwSurveyChunk.h"
class cwStation;
class cwShot;
class cwSurveyChunkViewComponents;
#include "cwStationReference.h"


class cwSurveyChunkView : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(cwSurveyChunk* model READ model WRITE setModel NOTIFY modelChanged)
    Q_ENUMS(DataBoxType)

public:

    explicit cwSurveyChunkView(QDeclarativeItem *parent = 0);
    ~cwSurveyChunkView();

    cwSurveyChunk* model();
    void setModel(cwSurveyChunk* chunk);

    void setFrontSights(bool hasFrontSights);
    void setBackSights(bool hasBackSights);

    QRectF boundingRect() const;

    static float elementHeight();
    static float heightHint(int numberElements);

    void setNavigationBelow(const cwSurveyChunkView* below);
    void setNavigationAbove(const cwSurveyChunkView* above);
    void setNavigation(const cwSurveyChunkView* above, const cwSurveyChunkView* below);

    void setQMLComponents(cwSurveyChunkViewComponents* components);

signals:
    void modelChanged();

    void createdNewChunk(cwSurveyChunk* chunk);
    void ensureVisibleChanged(QRectF rect);

public slots:

    void updateData(cwSurveyChunk::DataRole, int index, const QVariant& data);

protected:


private slots:
    void setFocusForFirstStation(bool focus);
    void setChildActiveFocus(bool focus);
    void stationValueHasChanged();

    void addStations(int beginIndex, int endIndex);
    void addShots(int beginIndex, int endIndex);

    void removeStations(int beginIndex, int endIndex);
    void removeShots(int beginIndex, int endIndex);

    void clear();

    void rightClickOnStation(int index);
    void rightClickOnShot(int index);

    void splitOnStation(int index);
    void splitOnShot(int index);

private:

    class Row {
    public:
        Row(int rowIndex, int numberOfItems);

        int rowIndex() const { return RowIndex; }
        QVector<QDeclarativeItem*> items() { return Items; }

    protected:
        QVector<QDeclarativeItem*> Items;
        int RowIndex;

        static QDeclarativeItem* setupItem(QDeclarativeComponent* component,
                                           QDeclarativeContext* context,
                                           cwSurveyChunk::DataRole,
                                           QValidator* validator);
    };

    class StationRow : public Row {
    public:

        StationRow();
        StationRow(cwSurveyChunkView* Chunk, int RowIndex);

        QDeclarativeItem* stationName() const { return Items[StationName]; }
        QDeclarativeItem* left() const { return Items[Left]; }
        QDeclarativeItem* right() const { return Items[Right]; }
        QDeclarativeItem* up() const { return Items[Up]; }
        QDeclarativeItem* down() const { return Items[Down]; }

    private:

        enum {
            StationName,
            Left,
            Right,
            Up,
            Down,
            NumberItems
        };
    };

    class ShotRow : public Row {
    public:
        ShotRow();
        ShotRow(cwSurveyChunkView* Chunk, int RowIndex);

        QDeclarativeItem* distance() const { return Items[Distance]; }
        QDeclarativeItem* frontCompass() const { return Items[FrontCompass]; }
        QDeclarativeItem* backCompass() const { return Items[BackCompass]; }
        QDeclarativeItem* frontClino() const { return Items[FrontClino]; }
        QDeclarativeItem* backClino() const { return Items[BackClino]; }

    private:
        enum {
            Distance,
            FrontCompass,
            BackCompass,
            FrontClino,
            BackClino,
            NumberItems
        };
    };

    friend class StationRow;
    friend class ShotRow;

    cwSurveyChunk* SurveyChunk;
    QList<StationRow> StationRows;
    QList<ShotRow> ShotRows;

    //This holds a reverse look up to station vs row and shot's verse rows
    //This is called when station data is changed for this view
    QMap<cwStationReference, int> StationToIndex;

    //Stations and shots are added to the navigation queue
    //When the are added and remove.  The navigation queue
    //The navigation queues are checked by UpdateNavigation()
    QList<int> StationNavigationQueue;
    QList<int> ShotNavigationQueue;

    //Where all the survey chunk view's delegates are stored
    cwSurveyChunkViewComponents* QMLComponents;

    QDeclarativeItem* StationTitle;
    QDeclarativeItem* DistanceTitle;
    QDeclarativeItem* AzimuthTitle;
    QDeclarativeItem* ClinoTitle;
    QDeclarativeItem* LeftTitle;
    QDeclarativeItem* RightTitle;
    QDeclarativeItem* UpTitle;
    QDeclarativeItem* DownTitle;

    QMenu* RightClickMenu;

    //For keeping the current object visible
    QDeclarativeItem* FocusedItem;

    //For showing front sites only or backsights only
    bool HasFrontSights;
    bool HasBackSights;

    //For setting the navigation for the last and first object
    const cwSurveyChunkView* ChunkBelow;
    const cwSurveyChunkView* ChunkAbove;

    void createTitlebar();

    void positionStationRow(StationRow row, int index);
    void positionElement(QDeclarativeItem* item, const QDeclarativeItem* titleItem, int index, int yOffset = 0, QSizeF size = QSizeF());

    void positionShotRow(ShotRow row, int index);

    void updateNavigation();
    void updateAllNavigation();
    void updateStationTabNavigation(int index);
    void updateShotTabNavigation(int index);
    void lrudTabNavigation(StationRow row, QDeclarativeItem* previous, QDeclarativeItem* next);
    void setTabOrder(QDeclarativeItem* item, QDeclarativeItem* previous, QDeclarativeItem* next);
    void updateStationArrowNavigation(int index);
    void updateShotArrowNavigaton(int index);
    void setArrowNavigation(QDeclarativeItem* item, QDeclarativeItem* left, QDeclarativeItem* right, QDeclarativeItem* up, QDeclarativeItem* down);

    ShotRow getShotRow(int index);
    ShotRow getNavigationShotRow(int index);
    StationRow getStationRow(int index);
    StationRow getNavigationStationRow(int index);

    void updateLastRowBehaviour();
    void updatePositionsAfterIndex(int index);
    void updateIndexes(int index);
    void updateDimensions();

    void updateShotData(cwSurveyChunk::DataRole role, int index, const QVariant& data);
    void updateStationData(cwSurveyChunk::DataRole role, int index, const QVariant& data);
    void updateStationRowData(int index);
    void updateShotRowData(int index);

    bool interfaceValid();

    QDeclarativeItem* leftBoxOfLeftLRUD(const ShotRow& shot);

};

//QML_DECLARE_TYPEINFO(cwSurveyChunkView, QML_HAS_ATTACHED_PROPERTIES)



#endif // CWSURVEYCHUCKVIEW_H
