
#include "Random.h"
#include <random>
#include <chrono>

static unsigned randomSeed = 1;

unsigned int Rand()
{
	static std::mt19937 randomGenerator32 = std::mt19937(std::chrono::system_clock::now().time_since_epoch().count());
    return randomGenerator32();
}

unsigned long long Rand64()
{
	static std::mt19937_64 randomGenerator64 = std::mt19937_64(std::chrono::system_clock::now().time_since_epoch().count());
	return randomGenerator64();
}

float RandStandardNormal()
{
    return ((unsigned short)Rand()) / 65535.0f;;
}
