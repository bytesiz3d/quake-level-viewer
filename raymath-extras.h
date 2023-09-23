#pragma once
#include <raymath.h>

inline static Vector3
operator+(Vector3 a, Vector3 b)
{
	return Vector3Add(a, b);
}

inline static Vector3
operator-(Vector3 a, Vector3 b)
{
	return Vector3Subtract(a, b);
}

inline static Vector3
operator-(Vector3 a)
{
	return Vector3Negate(a);
}

inline static Vector3
cross(Vector3 a, Vector3 b)
{
	return Vector3CrossProduct(a, b);
}

inline static float
dot(Vector3 a, Vector3 b)
{
	return Vector3DotProduct(a, b);
}

inline static Vector3
operator*(Vector3 a, float b)
{
	return Vector3Scale(a, b);
}

inline static Vector3
operator*(float a, Vector3 b)
{
	return b * a;
}

inline static Vector3
operator/(Vector3 a, float b)
{
	return Vector3Scale(a, 1.f / b);
}

// n . x + d = 0
struct Plane
{
	Vector3 n;
	float d;
};

inline static Plane
PlaneFromPoints(Vector3 v1, Vector3 v2, Vector3 v3)
{
	// Vertices are listed in counter-clockwise order
	// 0 ---------- 2
	// |
	// |
	// 1
	Plane p = {};
	p.n = Vector3Normalize(cross(v2 - v1, v3 - v1));
	p.d = -dot(p.n, v1);
	return p;
}

inline static float
PlaneSignedDistance(Plane p, Vector3 v)
{
	return dot(p.n, v) + p.d;
}

Vector3
PlaneProjectPoint(Plane p, Vector3 v)
{
	return v - PlaneSignedDistance(p, v) * p.n;
}

inline static bool
GetIntersectionPlanes(Plane p1, Plane p2, Plane p3, Vector3& out)
{
	float det = dot(p1.n, cross(p2.n, p3.n));
	if (det == 0)
		return false;

	out = (-p1.d * cross(p2.n, p3.n) +
		   -p2.d * cross(p3.n, p1.n) +
		   -p3.d * cross(p1.n, p2.n)) /
		  det;
	return true;
}

Vector3
BboxCenter(BoundingBox bbox)
{
	return (bbox.min + bbox.max) / 2.f;
}

Vector3
BboxSize(BoundingBox bbox)
{
	return {
		fabsf(bbox.max.x - bbox.min.x),
		fabsf(bbox.max.y - bbox.min.y),
		fabsf(bbox.max.z - bbox.min.z),
	};
}