#ifndef CWFUTUREMANAGERTOKEN_H
#define CWFUTUREMANAGERTOKEN_H

//Our includes
#include "cwFutureManagerModel.h"
#include "cwGlobals.h"

//This class allows for adding jobs to cwFutureManagerModel
//in a thread safe way.
class CAVEWHERE_LIB_EXPORT cwFutureManagerToken
{
public:
    cwFutureManagerToken(cwFutureManagerModel* model = nullptr);

    void addJob(const cwFutureManagerModel::Job& job);

private:
    QPointer<cwFutureManagerModel> Model;
};

#endif // CWFUTUREMANAGERTOKEN_H
