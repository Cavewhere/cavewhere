#ifndef CWSURVEYCHUNKGROUPVIEW_H
#define CWSURVEYCHUNKGROUPVIEW_H

//Qt includes
#include <QQuickItem>
#include <QSignalMapper>
class QModelIndex;

//Our includes
class cwTrip;
class cwSurveyChunkView;
class cwSurveyChunk;
class cwSurveyChunkViewComponents;
class cwSurveyChunkTrimmer;

class cwSurveyChunkGroupView : public QQuickItem
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
    explicit cwSurveyChunkGroupView(QQuickItem *parent = 0);

    void setTrip(cwTrip* trip);
    cwTrip* trip() const;

    float contentHeight() const;
    float contentWidth();

    float viewportX() const;
    float viewportY() const;
    float viewportWidth() const;
    float viewportHeight() const;

    void setViewportX(float x);
    void setViewportY(float y);
    void setViewportWidth(float width);
    void setViewportHeight(float height);

    /// The group view allows wants this rectangle visible
    /// Usually this is attached to a flickable qml object
    /// That keeps this rectangle visible
    Q_INVOKABLE QRectF ensureVisibleRect() const;

    Q_INVOKABLE void setFocus(int index);

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
    QSet<int> ChunkViewsWhiteList; //Prevents the chunk at index from being deleted

    QSignalMapper* NeedChunkAboveMapper;
    QSignalMapper* NeedChunkBelowMapper;

    //The viewport area, what is seen by the user
    QRectF ViewportArea;

    //A visible rectangle that the group view always want to be shown
    QRectF EnsureVisibleArea;

    //Enables the corret station shot interaction
    cwSurveyChunkTrimmer* ChunkTrimmer;

    void UpdatePosition(int index);
    void updateActiveChunkViews();
    void updateContentArea(int beginIndex, int endIndex);

    QPair<int, int> VisableRange();
    void CreateChunkView(int index);
    void DeleteChunkView(int index);



    void updateAboveBelowAndPosition(int index);

    void forceAllocateChunk(int chunkIndex, int allocatedChunkIndex);

private slots:
    //void InsertRows(int start, int end);
    void addChunks(int beginIndex, int endIndex);
    void removeChunks(int beginIndex, int endIndex);

    void UpdateChunkHeight();
    void SetEnsureVisibleRect(QRectF rect);

    void HandleSplitChunk(cwSurveyChunk* newChunk);

    void updateChunksFrontSights();
    void updateChunksBackSights();

    void forceAllocateChunkAbove(int index);
    void forceAllocateChunkBelow(int index);

};

//inline float cwSurveyChunkGroupView::contentHeight() const { return ContentArea.height(); }
//inline float cwSurveyChunkGroupView::contentWidth() const { return ContentArea.width(); }
inline float cwSurveyChunkGroupView::viewportX() const { return ViewportArea.x(); }
inline float cwSurveyChunkGroupView::viewportY() const { return ViewportArea.y(); }
inline float cwSurveyChunkGroupView::viewportWidth() const { return ViewportArea.width(); }
inline float cwSurveyChunkGroupView::viewportHeight() const { return ViewportArea.height(); }
inline QRectF cwSurveyChunkGroupView::ensureVisibleRect() const { return EnsureVisibleArea; }

#endif // CWSURVEYCHUNKGROUPVIEW_H
