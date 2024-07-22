#pragma once

#include <sp2/math/vector2.h>
#include "direction.h"
#include <functional>


class VisitFOV
{
public:
    VisitFOV(sp::Vector2i _center, int _radius, const std::function<bool(Direction, sp::Vector2i)>& _callback)
    : center(_center), radius(_radius), callback(_callback)
    {
        if (!callback(Direction::Up, center))
            return;
        visitFOVStep<Direction::Right>();
        visitFOVStep<Direction::Down>();
        visitFOVStep<Direction::Left>();
        visitFOVStep<Direction::Up>();
    }

private:
    template<Direction DIR> void visitFOVStep()
    {
        visitFOVStep<DIR, DIR - 1>(1, 0.0, 1.0f);
        visitFOVStep<DIR, DIR + 1>(1, 0.0, 1.0f);
    }
    template<Direction DIR, Direction SIDE> void visitFOVStep(int row, float fmin, float fmax)
    {
        if (row > radius)
            return;
        if (fmax - fmin < 0.001)
            return;
        auto dir = offset(DIR);
        auto side = offset(SIDE);
        for(int n=0; n<=row; n++) {
            auto start = (float(n) - 0.5f) / (float(row) + 0.5f);
            auto end = (float(n) + 0.5f) / (float(row) + 0.5f);

            if (end < fmin)
                continue;
            if (start > fmax)
                continue;

            auto offset = dir * row + side * n;
            auto p = center + offset;

            if (offset.x * offset.x + offset.y * offset.y < radius * radius) {
                if (!callback(DIR, p)) {
                    visitFOVStep<DIR, SIDE>(row + 1, fmin, start);
                    fmin = end;
                }
            } else {
                break;
            }
        }

        visitFOVStep<DIR, SIDE>(row + 1, fmin, fmax);
    }

    sp::Vector2i center;
    int radius;
    const std::function<bool(Direction, sp::Vector2i)>& callback;
};
