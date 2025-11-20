#ifndef CWDATAUTILITIES_H
#define CWDATAUTILITIES_H

#include <QList>

namespace cwData {

template <typename DataType, typename PtrType>
QList<DataType> toDataList(const QList<PtrType>& ptrList) {
    QList<DataType> datas;
    datas.reserve(ptrList.size());
    for(auto ptr : ptrList) {
        datas.append(ptr->data());
    }
    return datas;
}

}

#endif // CWDATAUTILITIES_H
