#ifndef CWSURVEYCHUNKGROUPVIEW_H
#define CWSURVEYCHUNKGROUPVIEW_H

//Qt includes
#include <QDeclarativeItem>
class QModelIndex;

//Our includes
class cwTrip;
class cwSurveyChunkView;
class cwSurveyChunk;
class cwSurveyChunkViewComponents;

class cwSurveyChunkGroupView : public QDeclarativeItem
{
    Q_OBJECT

    Q_PROPERTY(cwTrip* trip READ trip WRITE setTrip NOTIFY tripChanged)
    Q_PROPERTY(float contentHeight READ contentHeight NOTIFY contentHeightChanged )
    Q_PROPERTY(float contentWidth READ contentWidth NOTIFY contentWidthChanged )
    Q_PROPERTY(float viewportX READ viewportX WRITE setViewportX NOTIFY viewportXChanged)
    Q_PROPERTY(float viewportY READ viewportY WRITE setViewportY NOTIFY viewportYChanged)
    Q_PROPERTY(float viewportWidth READ viewportWidth WRITE setViewportWidth NOTIFY viewportWidthChanged)
    Q_PROPERTY(float viewportHeight READ viewportHeight WRITE setViewportHeight NOTIFY viewportHeightChanged)
    Q_PROPERTY(QRectF ensureVisibleRect READ ensureVisibleRect NOTIFY ensureVisibleRectChanged)

public:
    explicit cwSurveyChunkGroupView(QDeclarativeItem *parent = 0);

    void setTrip(cwTrip* trip);
    cwTrip* trip() const;

    Q_INVOKABLE float contentHeight() const;
    Q_INVOKABLE float contentWidth() const;

    Q_INVOKABLE float viewportX() const;
    Q_INVOKABLE float viewportY() const;
    Q_INVOKABLE float viewportWidth() const;
    Q_INVOKABLE float viewportHeight() const;

    Q_INVOKABLE void setViewportX(float x);
    Q_INVOKABLE void setViewportY(float y);
    Q_INVOKABLE void setViewportWidth(float width);
    Q_INVOKABLE void setViewportHeight(float height);

    /// The group view allows wants this rectangle visible
    /// Usually this is attached to a flickable qml object
    /// That keeps this rectangle visible
    Q_INVOKABLE QRectF ensureVisibleRect() const;

signals:
    void tripChanged();
    void contentHeightChanged();
    void contentWidthChanged();

    void viewportXChanged();
    void viewportYChanged();
    void viewportWidthChanged();
    void viewportHeightChanged();

    void ensureVisibleRectChanged();

public slots:

protected:
    void keyPressEvent(QKeyEvent *event);

private:
    //The model
    cwTrip* Trip;

    //GUI elements
    cwSurveyChunkViewComponents* ChunkQMLComponents; //The components for creating a chunk
    QList<cwSurveyChunkView*> ChunkViews; //The active chunks
    QList<QRectF> ChunkBoundingRects;  //Sorted in ascending order by y position
   // QList<float> ChunkYPositions; //All the chunk y positions, should be sorted ascending


    //The viewport area, what is seen by the user
    QRectF ViewportArea;

    //A visible rectangle that the group view always want to be shown
    QRectF EnsureVisibleArea;

    void UpdatePosition(int index);
    void UpdateActiveChunkViews();
    void UpdateContentArea(int beginIndex, int endIndex);

    QPair<int, int> VisableRange();
    void CreateChunkView(int index);
    void DeleteChunkView(int index);

    void SetFocus(int index);

private slots:
    //void InsertRows(int start, int end);
    void AddChunks(int beginIndex, int endIndex);

    void UpdateChunkHeight();
    void SetEnsureVisibleRect(QRectF rect);

    void HandleSplitChunk(cwSurveyChunk* newChunk);

};

//inline float cwSurveyChunkGroupView::contentHeight() const { return ContentArea.height(); }
//inline float cwSurveyChunkGroupView::contentWidth() const { return ContentArea.width(); }
inline float cwSurveyChunkGroupView::viewportX() const { return ViewportArea.x(); }
inline float cwSurveyChunkGroupView::viewportY() const { return ViewportArea.y(); }
inline float cwSurveyChunkGroupView::viewportWidth() const { return ViewportArea.width(); }
inline float cwSurveyChunkGroupView::viewportHeight() const { return ViewportArea.height(); }
inline QRectF cwSurveyChunkGroupView::ensureVisibleRect() const { return EnsureVisibleArea; }

#endif // CWSURVEYCHUNKGROUPVIEW_H
