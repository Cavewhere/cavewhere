/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWMARKER_H
#define CWMARKER_H

//Qt includes
#include <QList>
#include <QPointer>
#include <QString>
#include <QUuid>
#include <QVariantMap>
#include <QVector3D>

//Our includes
#include "cwKeyword.h"
#include "cwKeywordModel.h"

/**
 * @brief One line of text a cwMarkerView draws above a marker's sprite.
 *
 * styleKey names the cwMarkerTextStyle, and through it the text layer, that
 * draws the line. An empty line, or one whose style the view lacks, draws nothing.
 */
struct cwMarkerTextLine {
    QString text;
    QString styleKey;

    bool operator==(const cwMarkerTextLine&) const = default;
};

/**
 * @brief One marker a cwMarkerSource hands to a cwMarkerView.
 *
 * The view draws spriteIndex's atlas slot at position, stacks textLines above
 * it (the first line on top, the last line nearest the sprite), and creates one
 * delegate per marker with delegateProperties as its initial properties.
 *
 * Markers sharing a visibilityGroup share one keyword item: it extends
 * keywordExtension's keywords with keywords, and hiding it hides every marker
 * of the group. The group's first marker defines that item, so every marker of
 * a group carries the same keywordExtension and keywords. In the declutter pass
 * a lower declutterPriority claims its text space first; id breaks the last tie.
 * A source whose view claims no declutter space may leave id null.
 */
struct cwMarker {
    QUuid id;
    QVector3D position;
    int spriteIndex = 0;
    QList<cwMarkerTextLine> textLines;
    int declutterPriority = 0;
    QString visibilityGroup;
    QPointer<cwKeywordModel> keywordExtension;
    QList<cwKeyword> keywords;
    QVariantMap delegateProperties;
};

#endif // CWMARKER_H
