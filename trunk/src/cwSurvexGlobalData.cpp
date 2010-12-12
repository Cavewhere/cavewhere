#include "cwSurvexGlobalData.h"
#include "cwSurvexBlockData.h"

cwSurvexGlobalData::cwSurvexGlobalData(QObject* parent) :
    QObject(parent)
{

}

void cwSurvexGlobalData::setBlocks(QList<cwSurvexBlockData*> blocks) {
    foreach(cwSurvexBlockData* block, blocks) {
        block->setParent(this);
        block->setParentBlock(NULL);
    }

    RootBlocks = blocks;
}
