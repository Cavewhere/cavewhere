#include "varianceoverride.h"

namespace dewalls {

const QSharedPointer<VarianceOverride> VarianceOverride::FLOATED(new FloatedVarianceOverride());
const QSharedPointer<VarianceOverride> VarianceOverride::FLOATED_TRAVERSE(new FloatedTraverseVarianceOverride());

} // namespace dewalls

