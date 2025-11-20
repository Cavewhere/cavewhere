#include "cwReading.h"



void cwReading::setValue(const QString &value) {
    m_value = value;
    updateState();
}
