/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAVE_H
#define CWCAVE_H

//Our includes
#include "cwSurveyNode.h"

/**
 * A survey node the UI labels "Cave". Every node the loader, the importers and
 * addCave() construct is one, so qobject_cast<cwCave*> answers for any node in
 * the tree while the migration to cwSurveyNode runs; the class goes away once
 * the QML pages are typed SurveyNode (C8.2 of plans/SURVEY_TREE_PLAN.html).
 */
class CAVEWHERE_LIB_EXPORT cwCave : public cwSurveyNode
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Cave)

public:
    using cwSurveyNode::cwSurveyNode;
};

#endif // CWCAVE_H
