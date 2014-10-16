#include "wmhack.h"

Spaces::Spaces(const Geometry &top)
{
    emptySpace.insert(top);
}

Geometry
Spaces::fit(const Size &required)
{
    Geometry best;
    best.x = 2000;
    for (const auto &candidate : emptySpace)
        if (candidate.size.canContain(required) &&
                (best.x > candidate.x || best.x == candidate.x && best.y <= candidate.y))
            best = candidate;
    best.size = required;
    occlude(best);
    return best;
}

void
Spaces::occlude(const Geometry &take)
{
    // removing a rectangular area from another leaves up to four separate
    // rectangular regions left - to the top, left, right, and bottom of
    // the existing one.

    std::set<Geometry> result;
    for (const auto &clearing : emptySpace) {
        Geometry clippedRegion;

        unsigned endx = std::min(take.endx(), clearing.endx());
        unsigned endy = std::min(take.endy(), clearing.endy());
        clippedRegion.x = std::max(take.x, clearing.x);
        clippedRegion.y = std::max(take.y, clearing.y);
        clippedRegion.size.width = endx - clippedRegion.x;
        clippedRegion.size.height = endy - clippedRegion.y;

        if (clippedRegion.size.area() == 0) {
            result.insert(clearing); // no change - the clippedRegion area is disjoint from the clearing
        } else {
            if (clippedRegion.x > clearing.x) {
                // there's a region to the left of the clearing that's still visible
                result.insert(Geometry(Size(clippedRegion.x - clearing.x, clearing.size.height), clearing.x, clearing.y));
            }

            if (clippedRegion.endx() < clearing.endx()) {
                // there's a region to the right of the clearing that's still visible
                result.insert(Geometry(
                        Size(clearing.endx() - clippedRegion.endx(), clearing.size.height),
                        clippedRegion.endx(), clearing.y));
            }
            if (clippedRegion.y > clearing.y) {
                // there's a region to the top of the clearing that's still visible
                result.insert(Geometry(Size(clearing.size.width, clippedRegion.y - clearing.y), clearing.x, clearing.y));
            }
            if (clippedRegion.endy() < clearing.endx()) {
                // there's a region to the right of the clearing that's still visible
                result.insert(Geometry(
                        Size(clearing.size.width, clearing.endy() - clippedRegion.endy()),
                        clearing.x, clippedRegion.endy()));
            }
        }
    }
    std::swap(result, emptySpace);
}
