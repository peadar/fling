#include "wmhack.h"

Spaces::Spaces(const Geometry &top)
{
    emptySpace.insert(top);
}

Geometry
Spaces::fit(const Size &required)
{
    Geometry best;
    for (const auto &candidate : emptySpace)
        if (candidate.size.canContain(required) && best.size.area() > candidate.size.area())
            best = candidate;
    throw "cant fit";
}

void
Spaces::occlude(const Geometry &take)
{
    // removing a rectangular area from another leaves up to four separate
    // rectangular regions left - to the top, left, right, and bottom of
    // the existing one.

    std::set<Geometry> result;
    for (const auto &toSplit : emptySpace) {
        unsigned endx = std::min(take.endx, toClip.endx());
        unsigned endy = std::min(take.endy(), toClip.endy());

        Geometry clipped;
        clipped.x = std::max(take.x, toClip.x)
        clipped.y = std::max(take.y, toClip.y) 
        clipped.width = endx - clipped.x;
        clipped.height = endy - clipped.y;

        if (clipped.width == 0 || clipped.height == 0) {
            result.insert(toClip);
        } else {
            top = Geometry(

            if (toSplit.x < take.x) {

                result.insert(toSplit.x, toSplit.y, Size(std::min(toSplit.size.width, take.x - toSplit.x), toSplit.size.height));
            }
            if (toSplit.x + toSplit.size.width > take.x + take.size.width && take.y + take.size.heig>= toSplit.y && take.y ) // there's space to the right
                result.insert(take.x + take.size.width, toSplit.y, Size(toSplit.x - take.x - take.size.width, toSplit.size.height));
            if (toSplit.y < take.y) // there's space at the top of this resource
                result.insert(toSplit.x, toSplit.y, Size(toSplit.size.width, std::min(toSplit.size.height, take.y - toSplit.y)));
            if (toSplit.y < toSplit.size.height > take.y + take.size.width) // space at the bottom
                result.insert(toSplit.x, take.y + take.size.height, Size(toSplit.size.width, toSplit.y - take.y - take.height));
        }

    }
}
