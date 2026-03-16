
#pragma once 

#include "MathDefs.h"
#include "Vec3.h"
#include "Vec4.h"

NS_JYE_MATH_BEGIN

class Color 
{
public:
    float r, g, b, a;
public:
    Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
    Color(const Color& color, float na) : r(color.r), g(color.g), b(color.b), a(na) {}
    Color(float nr, float ng, float nb) : r(nr), g(ng), b(nb), a(1.0f) {}
    Color(float nr, float ng, float nb, float na) : r(nr), g(ng), b(nb), a(na) {}
    Color(const Color& color) = default;
	Color& operator =(const Color& rhs) = default;

    Color operator + (const Color& v) const { return Color(r + v.r, g + v.g, b + v.b, a + v.a); }
    Color operator - (const Color& v) const { return Color(r - v.r, g - v.g, b - v.b, a - v.a); }
	Color operator * (float k) const { return Color(r * k, g * k, b * k, a * k); }
	Color operator / (float k) const { return Color(r / k, g / k, b / k, a / k); }

    bool operator ==(const Color& rhs) const { return r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a; }
    bool operator !=(const Color& rhs) const { return r != rhs.r || g != rhs.g || b != rhs.b || a != rhs.a; }

    bool equals(const Color &other) const{ return (*this == other);}

    const float* getPtr() const { return &r; }
    void Set(float nr, float ng, float nb, float na) { r = nr, g = ng, b = nb, a = na; }

    unsigned toUInt() const;
    void fromUInt(unsigned color);

    Vec3 ToVector3() const { return Vec3(r, g, b); }
    Vec4 ToVector4() const { return Vec4(r, g, b, a); }
    void from(float x, float y, float z, float w) { r = x; g = y, b = z, w = a; }

    std::string toString() const;

    static const Color WHITE;
    static const Color YELLOW;
    static const Color BLUE;
    static const Color GREEN;
    static const Color RED;
    static const Color MAGENTA;
    static const Color BLACK;
    static const Color ORANGE;
    static const Color GRAY;
};

NS_JYE_MATH_END