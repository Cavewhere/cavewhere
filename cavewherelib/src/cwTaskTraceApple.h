#ifndef CWTASKTRACEAPPLE_H
#define CWTASKTRACEAPPLE_H

//Qt includes
#include <QtGlobal>

//Std includes
#include <optional>

//! Bytes the system default Metal device has allocated for this process, or std::nullopt without a device
std::optional<quint64> cwMetalAllocatedBytes();

#endif // CWTASKTRACEAPPLE_H
