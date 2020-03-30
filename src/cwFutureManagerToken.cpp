//Our includes
#include "cwFutureManagerToken.h"

//Qt includes
#include <QMetaObject>

cwFutureManagerToken::cwFutureManagerToken(cwFutureManagerModel *model) :
    Model(model)
{

}

void cwFutureManagerToken::addJob(const cwFutureManagerModel::Job &job)
{
    auto model = Model;

    QMetaObject::invokeMethod(model, [model, job]()
    {
        if(model) {
            model->addJob(job);
        }
    });
}
