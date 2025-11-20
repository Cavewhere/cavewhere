#ifndef CWKEYWORDVISIBILITY_H
#define CWKEYWORDVISIBILITY_H

//Qt includes
#include <QAbstractItemModel>
#include <QObject>
#include <QPointer>
#include <QQMLEngine>

//Our includes
#include "cwGlobals.h"
class cwKeywordItemModel;
class cwRenderObject;

class CAVEWHERE_LIB_EXPORT cwKeywordVisibility : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(KeywordVisibility)

    Q_PROPERTY(QAbstractItemModel* visibleModel READ visibleModel WRITE setVisibleModel NOTIFY visibleModelChanged)
    Q_PROPERTY(QAbstractItemModel* hideModel READ hideModel WRITE setHideModel NOTIFY hideModelChanged)

public:
    explicit cwKeywordVisibility(QObject *parent = nullptr);

    QAbstractItemModel* visibleModel() const;
    void setVisibleModel(QAbstractItemModel* visibleModel);

    QAbstractItemModel* hideModel() const;
    void setHideModel(QAbstractItemModel* hideModel);


signals:
    void visibleModelChanged();
    void hideModelChanged();

private:
    QAbstractItemModel* mVisibleModel = nullptr; //!<
    QAbstractItemModel* mHideModel = nullptr; //!<

    void connectModel(QAbstractItemModel* model, bool visible);
    void applyVisibility(QAbstractItemModel* model, bool visible) const;

};

inline QAbstractItemModel* cwKeywordVisibility::visibleModel() const {
    return mVisibleModel;
}

inline QAbstractItemModel* cwKeywordVisibility::hideModel() const {
    return mHideModel;
}

#endif // CWKEYWORDVISIBILITY_H
