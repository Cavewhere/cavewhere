#include "cwKeywordComponent.h"
#include "cwKeywordModel.h"

/**
*
*/
cwKeywordComponent::cwKeywordComponent(Qt3DCore::QNode *node) :
    Qt3DCore::QComponent(node),
    KeywordModel(new cwKeywordModel(this))
{

}

