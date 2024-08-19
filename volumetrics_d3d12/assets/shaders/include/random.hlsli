#ifndef RANDOM_H
#define RANDOM_H

// From Unreal Engine
uint4 Rand4DPCG32(int4 p)
{
	// taking a signed int then reinterpreting as unsigned gives good behavior for negatives
	uint4 v = uint4(p);

	// Linear congruential step.
	v = v * 1664525u + 1013904223u;

	// shuffle
	v.x += v.y * v.w;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v.w += v.y * v.z;

	// xoring high bits into low makes all 32 bits pretty good
	v ^= (v >> 16u);

	// final shuffle
	v.x += v.y * v.w;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v.w += v.y * v.z;

	return v;
}

#endif