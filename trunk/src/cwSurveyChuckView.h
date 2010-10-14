#ifndef CWSURVEYCHUCKVIEW_H
#define CWSURVEYCHUCKVIEW_H

//Qt includes
#include <QObject>
#include <QDeclarativeItem>

//Our includes
class cwSurveyChunk;
class cwStation;
class cwShot;

class cwSurveyChuckView : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(cwSurveyChunk* model READ model WRITE setModel NOTIFY modelChanged)

public:
    explicit cwSurveyChuckView(QDeclarativeItem *parent = 0);

    cwSurveyChunk* model();
    void setModel(cwSurveyChunk* chunk);

signals:
    void modelChanged();

public slots:
    void AddStations(int beginIndex, int endIndex);
    void AddShots(int beginIndex, int endIndex);

    void Clear();

private:

    class StationRow {
    public:

        StationRow(cwSurveyChuckView* Chunk);

        QDeclarativeItem* station() { return Station; }
        QDeclarativeItem* left() { return Left; }
        QDeclarativeItem* right() { return Right; }
        QDeclarativeItem* up() { return Up; }
        QDeclarativeItem* down() { return Down; }

        private:
        QDeclarativeItem* Station;
        QDeclarativeItem* Left;
        QDeclarativeItem* Right;
        QDeclarativeItem* Up;
        QDeclarativeItem* Down;

    };

    class ShotRow {
    public:
        ShotRow(cwSurveyChuckView* Chunk);

        QDeclarativeItem* distance() { return Distance; }
        QDeclarativeItem* frontCompass() { return FrontCompass; }
        QDeclarativeItem* backCompass() { return BackCompass; }
        QDeclarativeItem* frontClino() { return FrontClino; }
        QDeclarativeItem* backClino() { return BackClino; }

    private:
        QDeclarativeItem* Distance;
        QDeclarativeItem* FrontCompass;
        QDeclarativeItem* BackCompass;
        QDeclarativeItem* FrontClino;
        QDeclarativeItem* BackClino;

    };

    friend class StationRow;
    friend class ShotRow;

    cwSurveyChunk* Model;
    QList<StationRow*> StationRows;
    QList<ShotRow*> ShotRows;

    QDeclarativeComponent* StationDelegate;
    QDeclarativeComponent* TitleDelegate;
    QDeclarativeComponent* LeftDelegate;
    QDeclarativeComponent* RightDelegate;
    QDeclarativeComponent* UpDelegate;
    QDeclarativeComponent* DownDelegate;
    QDeclarativeComponent* DistanceDelegate;
    QDeclarativeComponent* FrontCompassDelegate;
    QDeclarativeComponent* BackCompassDelegate;
    QDeclarativeComponent* FrontClinoDelegate;
    QDeclarativeComponent* BackClinoDelegate;


    QDeclarativeItem* StationTitle;
    QDeclarativeItem* DistanceTitle;
    QDeclarativeItem* AzimuthTitle;
    QDeclarativeItem* ClinoTitle;
    QDeclarativeItem* LeftTitle;
    QDeclarativeItem* RightTitle;
    QDeclarativeItem* UpTitle;
    QDeclarativeItem* DownTitle;


    void CreateTitlebar();
    void SetupDelegates();

    void PositionStationRow(StationRow* row, int index);
    void PositionElement(QDeclarativeItem* item, QDeclarativeItem* titleItem, int index, int yOffset = 0, QSizeF size = QSizeF());
    void ConnectStation(cwStation* station, StationRow* row);

    void PositionShotRow(ShotRow* row, int index);
    void ConnectShot(cwShot* shot, ShotRow* row);

};



#endif // CWSURVEYCHUCKVIEW_H
