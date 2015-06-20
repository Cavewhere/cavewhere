#include "varianceoverride.h"

namespace dewalls {

FloatedVarianceOverride * const VarianceOverride::FLOATED = new FloatedVarianceOverride();
FloatedTraverseVarianceOverride * const VarianceOverride::FLOATED_TRAVERSE = new FloatedTraverseVarianceOverride();

} // namespace dewalls

