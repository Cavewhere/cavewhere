#include "cwClinoReading.h"
#include "cwClinoValidator.h"

void cwClinoReading::updateState() {
    const QString value = this->value();
    if (value.isEmpty()) {
        m_state = State::Empty;
    } else if (value.compare("down", Qt::CaseInsensitive) == 0) {
        m_state = State::Down;
    } else if (value.compare("up", Qt::CaseInsensitive) == 0) {
        m_state = State::Up;
    } else {
        bool ok;
        double numberValue = value.toDouble(&ok);
        if(ok && cwClinoValidator::check(numberValue)) {
            m_state = State::Valid;
        } else {
            m_state = State::Invalid;
        }
    }
}
