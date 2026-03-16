#include "Color.h"

NS_JYE_MATH_BEGIN

const Color Color::WHITE  (    1,     1,     1, 1);
const Color Color::YELLOW (    1,     1,     0, 1);
const Color Color::GREEN  (    0,     1,     0, 1);
const Color Color::BLUE   (    0,     0,     1, 1);
const Color Color::RED    (    1,     0,     0, 1);
const Color Color::MAGENTA(    1,     0,     1, 1);
const Color Color::BLACK  (    0,     0,     0, 1);
const Color Color::ORANGE (    1,  0.5f,     0, 1);
const Color Color::GRAY   (0.65f, 0.65f, 0.65f, 1);

unsigned Color::toUInt() const
{
    auto nr = (unsigned)Clamp(((int)(r * 255.0f)), 0, 255);
    auto ng = (unsigned)Clamp(((int)(g * 255.0f)), 0, 255);
    auto nb = (unsigned)Clamp(((int)(b * 255.0f)), 0, 255);
    auto na = (unsigned)Clamp(((int)(a * 255.0f)), 0, 255);
    return (nr << 24u) | (ng << 16u) | (nb << 8u) | na;
}

void Color::fromUInt(unsigned color)
{
    a = ((color >> 24u) & 0xffu) / 255.0f;
    b = ((color >> 16u) & 0xffu) / 255.0f;
    g = ((color >> 8u)  & 0xffu) / 255.0f;
    r = ((color >> 0u)  & 0xffu) / 255.0f;
}

std::string Color::toString() const
{
    char tempBuffer[256];
    sprintf(tempBuffer, "%g %g %g %g", r, g, b, a);
    return std::string(tempBuffer);
}

NS_JYE_MATH_END