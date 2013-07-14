/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSURVEYCHUCKVIEW_H
#define CWSURVEYCHUCKVIEW_H

//Qt includes
#include <QObject>
#include <QQuickItem>
#include <QList>
#include <QVector>
#include <QDebug>
class QValidator;

//Our includes
#include "cwSurveyChunk.h"
class cwStation;
class cwShot;
class cwSurveyChunkViewComponents;
class cwValidator;
class cwSurveyChunkTrimmer;


class cwSurveyChunkView : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(cwSurveyChunk* model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(cwSurveyChunkTrimmer* chunkTrimmer READ chunkTrimmer WRITE setChunkTrimmer NOTIFY chunkTrimmerChanged)
    Q_ENUMS(DataBoxType)

public:

    explicit cwSurveyChunkView(QQuickItem *parent = 0);
    ~cwSurveyChunkView();

    cwSurveyChunk* model() const;
    void setModel(cwSurveyChunk* chunk);

    cwSurveyChunkTrimmer* chunkTrimmer() const;
    void setChunkTrimmer(cwSurveyChunkTrimmer* chunkTrimmer);

    void setFrontSights(bool hasFrontSights);
    void setBackSights(bool hasBackSights);

    QRectF boundingRectangle();

    static float elementHeight();
    static float heightHint(int numberElements);

    void setNavigationBelow(const cwSurveyChunkView* below);
    void setNavigationAbove(const cwSurveyChunkView* above);
    void setNavigation(const cwSurveyChunkView* above, const cwSurveyChunkView* below);

    void setQMLComponents(cwSurveyChunkViewComponents* components);

    Q_INVOKABLE void tab(int rowIndex, int role);
    Q_INVOKABLE void previousTab(int rowIndex, int role);
    Q_INVOKABLE void navigationArrow(int rowIndex, int role, int key);
    Q_INVOKABLE void ensureDataBoxVisible(int rowIndex, int role);

    Q_INVOKABLE void showRemoveBoxsOnStations(int begin, int end);
    Q_INVOKABLE void showRemoveBoxsOnShots(int begin, int end);
    Q_INVOKABLE void hideRemoveBoxs();

signals:
    void modelChanged();

    void createdNewChunk(cwSurveyChunk* chunk);
    void ensureVisibleChanged(QRectF rect);

    void needChunkAbove(); //Always hook up with a direct connection
    void needChunkBelow(); //Always hook up with a direct connection

    void chunkTrimmerChanged();

public slots:

    void updateData(cwSurveyChunk::DataRole, int index);

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
        QVector<QQuickItem*> items() { return Items; }

    protected:
        QVector<QQuickItem*> Items;
        int RowIndex;

        static QQuickItem* setupItem(QQmlComponent* component,
                                           QQmlContext* context,
                                           cwSurveyChunk::DataRole,
                                           cwValidator* validator);
    };

    class StationRow : public Row {
    public:

        StationRow();
        StationRow(cwSurveyChunkView* Chunk, int RowIndex);

        QQuickItem* stationName() const { return Items[StationName]; }
        QQuickItem* left() const { return Items[Left]; }
        QQuickItem* right() const { return Items[Right]; }
        QQuickItem* up() const { return Items[Up]; }
        QQuickItem* down() const { return Items[Down]; }

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

        QQuickItem* distance() const { return Items[Distance]; }
        QQuickItem* frontCompass() const { return Items[FrontCompass]; }
        QQuickItem* backCompass() const { return Items[BackCompass]; }
        QQuickItem* frontClino() const { return Items[FrontClino]; }
        QQuickItem* backClino() const { return Items[BackClino]; }

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
    cwSurveyChunkTrimmer* ChunkTrimmer;

    QList<StationRow> StationRows;
    QList<ShotRow> ShotRows;

    //Where all the survey chunk view's delegates are stored
    cwSurveyChunkViewComponents* QMLComponents;

    QQuickItem* StationTitle;
    QQuickItem* DistanceTitle;
    QQuickItem* AzimuthTitle;
    QQuickItem* ClinoTitle;
    QQuickItem* LeftTitle;
    QQuickItem* RightTitle;
    QQuickItem* UpTitle;
    QQuickItem* DownTitle;

//    QMenu* RightClickMenu;

    //For keeping the current object visible
    QQuickItem* FocusedItem;

    //For showing front sites only or backsights only
    bool HasFrontSights;
    bool HasBackSights;

    //For setting the navigation for the last and first object
    const cwSurveyChunkView* ChunkBelow;
    const cwSurveyChunkView* ChunkAbove;

    void createTitlebar();

    void positionStationRow(StationRow row, int index);
    void positionElement(QQuickItem* item, const QQuickItem* titleItem, int index, int yOffset = 0, QSizeF size = QSizeF());

    void positionShotRow(ShotRow row, int index);

    ShotRow getShotRow(int index);
    ShotRow getNavigationShotRow(int index);
    StationRow getStationRow(int index);
    StationRow getNavigationStationRow(int index);

//    void updateLastRowBehaviour();
    void updatePositionsAfterIndex(int index);
    void updateIndexes(int index);
    void updateDimensions();

    void updateShotData(cwSurveyChunk::DataRole role, int index);
    void updateStationData(cwSurveyChunk::DataRole role, int index);
    void updateStationRowData(int index);
    void updateShotRowData(int index);

    bool interfaceValid();

    QQuickItem* tabFromStation(int rowIndex);
    QQuickItem* tabFromClino(int rowIndex);
    QQuickItem* tabFromBackClino(int rowIndex);
    QQuickItem* tabFromClinoToLRUD(int rowIndex);
    QQuickItem* tabFromDown(int rowIndex);
    QQuickItem* previousTabFromStation(int rowIndex);
    QQuickItem* previousTabFromLeft(int rowIndex);

    QQuickItem* navArrowLeft(int rowIndex, int role);
    QQuickItem* navArrowRight(int rowIndex, int role);
    QQuickItem* navArrowUp(int rowIndex, int role);
    QQuickItem* navArrowDown(int rowIndex, int role);
    QQuickItem* navArrowUpDown(int rowIndex, int role, Qt::Key key);

    void setItemFocus(QQuickItem* item);

    QQuickItem* databox(int rowIndex, int role);

    void removeBoxVisible(bool visible, StationRow row);
    void removeBoxVisible(bool visible, ShotRow row);

};

Q_DECLARE_METATYPE(cwSurveyChunkView*)

//QML_DECLARE_TYPEINFO(cwSurveyChunkView, QML_HAS_ATTACHED_PROPERTIES)

inline cwSurveyChunkTrimmer* cwSurveyChunkView::chunkTrimmer() const {
    return ChunkTrimmer;
}


#endif // CWSURVEYCHUCKVIEW_H
