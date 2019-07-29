#ifndef CWKEYWORDCOMPONENT_H
#define CWKEYWORDCOMPONENT_H

//Qt3d includes
#include <QComponent>

//Our includes
#include "cwGlobals.h"
class cwKeywordModel;

class CAVEWHERE_LIB_EXPORT cwKeywordComponent : public Qt3DCore::QComponent
{
    Q_OBJECT

    Q_PROPERTY(cwKeywordModel* keywordModel READ keywordModel CONSTANT)

public:
    cwKeywordComponent(Qt3DCore::QNode* node = nullptr);

    cwKeywordModel* keywordModel() const;

private:
    cwKeywordModel* KeywordModel = nullptr; //!<

};

/**
*
*/
inline cwKeywordModel* cwKeywordComponent::keywordModel() const {
    return KeywordModel;
}

#endif // CWKEYWORDCOMPONENT_H
