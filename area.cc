#include "wmhack.h"

Spaces::Spaces(const Geometry &top)
{
    emptySpace.insert(top);
}

Geometry
Spaces::fit(const Size &required)
{
    Geometry best;
    for (const auto &candidate : emptySpace) {
        if (candidate.size.canContain(required) && best.size.area() > candidate.size.area())
            best = candidate;
    }
    throw "cant fit";
}


void
Spaces::occlude(const Geometry &)
{
}

