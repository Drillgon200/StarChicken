#pragma once
#include <stdint.h>

template<uint8_t Size, typename T, typename Vec, uint8_t A>
struct swizzle1 {
	T components[Size];
	Vec& operator=(const Vec& other) {
		components[A] = other.components[A];
		return *this;
	}
	operator T() {
		return components[A];
	}
};

template<uint8_t Size, typename T, typename Vec, uint8_t A, uint8_t B>
struct swizzle2 {
	T components[Size];
	Vec& operator=(const Vec& other) {
		components[A] = other.components[A];
		components[B] = other.components[B];
		return *this;
	}
	operator Vec() {
		return Vec(components[A], components[B]);
	}
};

template<uint8_t Size, typename T, typename Vec, uint8_t A, uint8_t B, uint8_t C>
struct swizzle3 {
	T components[Size];
	Vec& operator=(const Vec& other) {
		components[A] = other.components[A];
		components[B] = other.components[B];
		components[C] = other.components[C];
		return *this;
	}
	operator Vec() {
		return Vec(components[A], components[B], components[C]);
	}
};

template<typename T>
class vec2 {
public:
	T components[2];
};
using vec2f = vec2<float>;

template<typename T>
class vec3 {
public:
	T components[3];
};
using vec3f = vec3<float>;

template<typename T>
class vec4 {
public:
	union {
		T components[4];
		//What must I do to get swizzling around here?!?
		//If this is it, fine, I'll do it. Looks like that's what GLM's doing, too, and that's a good library.
		swizzle1<4, T, T, 0> x, r, s;
		swizzle1<4, T, T, 1> y, g, t;
		swizzle1<4, T, T, 2> z, b, p;
		swizzle1<4, T, T, 3> w, a, q;

		swizzle2<4, T, vec2f, 0, 0> xx, rr, ss;
		swizzle2<4, T, vec2f, 0, 1> xy, rg, st;
		swizzle2<4, T, vec2f, 0, 2> xz, rb, sp;
		swizzle2<4, T, vec2f, 0, 3> xw, ra, sq;
		swizzle2<4, T, vec2f, 1, 0> yx, gr, ts;
		swizzle2<4, T, vec2f, 1, 1> yy, gg, tt;
		swizzle2<4, T, vec2f, 1, 2> yz, gb, tp;
		swizzle2<4, T, vec2f, 1, 3> yw, ga, tq;
		swizzle2<4, T, vec2f, 2, 0> zx, br, ps;
		swizzle2<4, T, vec2f, 2, 1> zy, bg, pt;
		swizzle2<4, T, vec2f, 2, 2> zz, bb, pp;
		swizzle2<4, T, vec2f, 2, 3> zw, ba, pq;
		swizzle2<4, T, vec2f, 3, 0> wx, ar, qs;
		swizzle2<4, T, vec2f, 3, 1> wy, ag, qt;
		swizzle2<4, T, vec2f, 3, 2> wz, ab, qp;
		swizzle2<4, T, vec2f, 3, 3> ww, aa, qq;

		swizzle3<4, T, vec3f, 0, 0, 0> xxx, rrr, sss;
		swizzle3<4, T, vec3f, 0, 0, 1> xxy, rrg, sst;
		swizzle3<4, T, vec3f, 0, 0, 2> xxz, rrb, ssp;
		swizzle3<4, T, vec3f, 0, 0, 3> xxw, rra, ssq;
		swizzle3<4, T, vec3f, 0, 1, 0> xyx, rgr, sts;
		swizzle3<4, T, vec3f, 0, 1, 1> xyy, rgg, stt;
		swizzle3<4, T, vec3f, 0, 1, 2> xyz, rgb, stp;
		swizzle3<4, T, vec3f, 0, 1, 3> xyw, rga, stq;
		swizzle3<4, T, vec3f, 0, 2, 0> xzx, rbr, sps;
		swizzle3<4, T, vec3f, 0, 2, 1> xzy, rbg, spt;
		swizzle3<4, T, vec3f, 0, 2, 2> xzz, rbb, spp;
		swizzle3<4, T, vec3f, 0, 2, 3> xzw, rba, spq;
		swizzle3<4, T, vec3f, 0, 3, 0> xwx, rar, sqs;
		swizzle3<4, T, vec3f, 0, 3, 1> xwy, rag, sqt;
		swizzle3<4, T, vec3f, 0, 3, 2> xwz, rab, sqp;
		swizzle3<4, T, vec3f, 0, 3, 3> xww, raa, sqq;
	};

	vec4(T all) {
		components[0] = all;
		components[1] = all;
		components[2] = all;
		components[3] = all;
	}

	vec4(T x, T y, T z, T a) {
		components[0] = x;
		components[1] = y;
		components[2] = z;
		components[3] = a;
	}
};
using vec4f = vec4<float>;