#pragma once

#include <array>


template<typename T, int X, int Y> class Array2 {
public:
    T& operator[](sp::Vector2i p) {
        sp2assert(p.x >= 0, "pos violation");
        sp2assert(p.y >= 0, "pos violation");
        sp2assert(p.x < X, "pos violation");
        sp2assert(p.y < Y, "pos violation");
        return buffer[p.x+p.y*X];
    }

    void fill(const T& v) {
        for(int n=0; n<X*Y; n++)
            buffer[n] = v;
    }

    static inline constexpr int W = X;
    static inline constexpr int H = Y;

    auto begin() { return buffer.begin(); }
    auto end() { return buffer.end(); }
private:
    std::array<T, X*Y> buffer;
};
