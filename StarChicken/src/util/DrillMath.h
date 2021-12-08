#pragma once
#include <stdint.h>
#include <math.h>
#include <assert.h>

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
	union {
		T components[3];
		swizzle1<3, T, T, 0> x, r, s;
		swizzle1<3, T, T, 1> y, g, t;
		swizzle1<3, T, T, 2> z, b, p;
	};

	vec3() {
		components[0] = 0.0;
		components[1] = 0.0;
		components[2] = 0.0;
	}

	vec3(T x, T y, T z) {
		components[0] = x;
		components[1] = y;
		components[2] = z;
	}
	vec3(T all) {
		components[0] = all;
		components[1] = all;
		components[2] = all;
	}
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
		//Yeah maybe I'll automate this later when I actually need swizzling
	};

	vec4() {
		components[0] = 0.0;
		components[1] = 0.0;
		components[2] = 0.0;
		components[3] = 0.0;
	}

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
using vec4d = vec4<double>;
using vec4i64 = vec4<int64_t>;
using vec4ui64 = vec4<uint64_t>;
using vec4i32 = vec4<int32_t>;
using vec4ui32 = vec4<uint32_t>;
using vec4i16 = vec4<int16_t>;
using vec4ui16 = vec4<uint16_t>;
using vec4i8 = vec4<int8_t>;
class vec4ui8 : vec4<uint8_t> {
public:
	using vec4<uint8_t>::vec4;
	//Overload for colors
	vec4ui8(float x, float y, float z, float w) :
		vec4<uint8_t>(static_cast<uint8_t>(x * 255.0F), static_cast<uint8_t>(y * 255.0F), static_cast<uint8_t>(z * 255.0F), static_cast<uint8_t>(w * 255.0F)) {
	}
};

//Collumn major 4x4 matrix
template<typename T>
struct mat4 {
	T mat[16];

	mat4() {
		set_identity();
	}

	mat4(T* values) {
		memcpy(mat, values, 16 * sizeof(T));
	}

	T* operator[](int32_t idx) {
		assert(idx >= 0);
		return &mat[idx * 4];
	}

	mat4<T>& set_identity() {
		mat[0] = 1;
		mat[1] = 0;
		mat[2] = 0;
		mat[3] = 0;

		mat[4] = 0;
		mat[5] = 1;
		mat[6] = 0;
		mat[7] = 0;

		mat[8] = 0;
		mat[9] = 0;
		mat[10] = 1;
		mat[11] = 0;

		mat[12] = 0;
		mat[13] = 0;
		mat[14] = 0;
		mat[15] = 1;

		return *this;
	}

	mat4<T> set_zero() {
		memset(mat, 0, 16*sizeof(T));
		return *this;
	}

	mat4<T>& set(mat4<T>& other) {
		memcpy(mat, other.mat, 16 * sizeof(T));
		return *this;
	}

	mat4<T>& set(T* values) {
		memcpy(mat, values, 16 * sizeof(T));
		return *this;
	}

	mat4<T>& mul(mat4<T>& other, mat4<T>& dest) {
		T temp[16];
		//TODO replace with AVX intrinsics
		temp[0] = mat[0] * other.mat[0] + mat[4] * other.mat[1] + mat[8] * other.mat[2] + mat[12] * other.mat[3];
		temp[1] = mat[1] * other.mat[0] + mat[5] * other.mat[1] + mat[9] * other.mat[2] + mat[13] * other.mat[3];
		temp[2] = mat[2] * other.mat[0] + mat[6] * other.mat[1] + mat[10] * other.mat[2] + mat[14] * other.mat[3];
		temp[3] = mat[3] * other.mat[0] + mat[7] * other.mat[1] + mat[11] * other.mat[2] + mat[15] * other.mat[3];

		temp[4] = mat[0] * other.mat[4] + mat[4] * other.mat[5] + mat[8] * other.mat[6] + mat[12] * other.mat[7];
		temp[5] = mat[1] * other.mat[4] + mat[5] * other.mat[5] + mat[9] * other.mat[6] + mat[13] * other.mat[7];
		temp[6] = mat[2] * other.mat[4] + mat[6] * other.mat[5] + mat[10] * other.mat[6] + mat[14] * other.mat[7];
		temp[7] = mat[3] * other.mat[4] + mat[7] * other.mat[5] + mat[11] * other.mat[6] + mat[15] * other.mat[7];

		temp[8] = mat[0] * other.mat[8] + mat[4] * other.mat[9] + mat[8] * other.mat[10] + mat[12] * other.mat[11];
		temp[9] = mat[1] * other.mat[8] + mat[5] * other.mat[9] + mat[9] * other.mat[10] + mat[13] * other.mat[11];
		temp[10] = mat[2] * other.mat[8] + mat[6] * other.mat[9] + mat[10] * other.mat[10] + mat[14] * other.mat[11];
		temp[11] = mat[3] * other.mat[8] + mat[7] * other.mat[9] + mat[11] * other.mat[10] + mat[15] * other.mat[11];

		temp[12] = mat[0] * other.mat[12] + mat[4] * other.mat[13] + mat[8] * other.mat[14] + mat[12] * other.mat[15];
		temp[13] = mat[1] * other.mat[12] + mat[5] * other.mat[13] + mat[9] * other.mat[14] + mat[13] * other.mat[15];
		temp[14] = mat[2] * other.mat[12] + mat[6] * other.mat[13] + mat[10] * other.mat[14] + mat[14] * other.mat[15];
		temp[15] = mat[3] * other.mat[12] + mat[7] * other.mat[13] + mat[11] * other.mat[14] + mat[15] * other.mat[15];

		dest.set(temp);
		return dest;
	}

	mat4<T>& mul(mat4<T>& other) {
		return mul(other, *this);
	}

	T determinant() {
		T det = mat[0 * 4 + 0] * ((mat[1 * 4 + 1] * mat[2 * 4 + 2] * mat[3 * 4 + 3] + mat[1 * 4 + 2] * mat[2 * 4 + 3] * mat[3 * 4 + 1] + mat[1 * 4 + 3] * mat[2 * 4 + 1] * mat[3 * 4 + 2])
			- mat[1 * 4 + 3] * mat[2 * 4 + 2] * mat[3 * 4 + 1] - mat[1 * 4 + 2] * mat[2 * 4 + 1] * mat[3 * 4 + 3] - mat[1 * 4 + 1] * mat[2 * 4 + 3] * mat[3 * 4 + 2]);

		det -= mat[1 * 4 + 0] * ((mat[0 * 4 + 1] * mat[2 * 4 + 2] * mat[3 * 4 + 3] + mat[0 * 4 + 2] * mat[2 * 4 + 3] * mat[3 * 4 + 1] + mat[0 * 4 + 3] * mat[2 * 4 + 1] * mat[3 * 4 + 2])
			- mat[0 * 4 + 3] * mat[2 * 4 + 2] * mat[3 * 4 + 1] - mat[0 * 4 + 2] * mat[2 * 4 + 1] * mat[3 * 4 + 3] - mat[0 * 4 + 1] * mat[2 * 4 + 3] * mat[3 * 4 + 2]);

		det += mat[2 * 4 + 0] * ((mat[0 * 4 + 1] * mat[1 * 4 + 2] * mat[3 * 4 + 3] + mat[0 * 4 + 2] * mat[1 * 4 + 3] * mat[3 * 4 + 1] + mat[0 * 4 + 3] * mat[1 * 4 + 1] * mat[3 * 4 + 2])
			- mat[0 * 4 + 3] * mat[1 * 4 + 2] * mat[3 * 4 + 1] - mat[0 * 4 + 2] * mat[1 * 4 + 1] * mat[3 * 4 + 3] - mat[0 * 4 + 1] * mat[1 * 4 + 3] * mat[3 * 4 + 2]);

		det -= mat[3 * 4 + 0] * ((mat[0 * 4 + 1] * mat[1 * 4 + 2] * mat[2 * 4 + 3] + mat[0 * 4 + 2] * mat[1 * 4 + 3] * mat[2 * 4 + 1] + mat[0 * 4 + 3] * mat[1 * 4 + 1] * mat[2 * 4 + 2])
			- mat[0 * 4 + 3] * mat[1 * 4 + 2] * mat[2 * 4 + 1] - mat[0 * 4 + 2] * mat[1 * 4 + 1] * mat[2 * 4 + 3] - mat[0 * 4 + 1] * mat[1 * 4 + 3] * mat[2 * 4 + 2]);
		return det;
	}

	T determinant3x3(T m00, T m01, T m02, T m10, T m11, T m12, T m20, T m21, T m22) {
		return m00 * (m11 * m22 - m12 * m21) + m01 * (m12 * m20 - m10 * m22) + m02 * (m10 * m21 - m11 * m20);
	}

	//https://semath.info/src/inverse-cofactor-ex4.html
	mat4<T>& inverse(mat4<T>& dest) {
		T det = determinant();
		if (det != 0) {
			T detInv = 1.0F / det;
			T temp[16];
			//Row 1
			temp[0] = determinant3x3(mat[1 * 4 + 1], mat[1 * 4 + 2], mat[1 * 4 + 3], mat[2 * 4 + 1], mat[2 * 4 + 2], mat[2 * 4 + 3], mat[3 * 4 + 1], mat[3 * 4 + 2], mat[3 * 4 + 3]) * detInv;
			temp[1] = (-determinant3x3(mat[1 * 4 + 0], mat[1 * 4 + 2], mat[1 * 4 + 3], mat[2 * 4 + 0], mat[2 * 4 + 2], mat[2 * 4 + 3], mat[3 * 4 + 0], mat[3 * 4 + 2], mat[3 * 4 + 3])) * detInv;
			temp[3] = determinant3x3(mat[1 * 4 + 0], mat[1 * 4 + 1], mat[1 * 4 + 3], mat[2 * 4 + 0], mat[2 * 4 + 1], mat[2 * 4 + 3], mat[3 * 4 + 0], mat[3 * 4 + 1], mat[3 * 4 + 3]) * detInv;
			temp[3] = (-determinant3x3(mat[1 * 4 + 0], mat[1 * 4 + 1], mat[1 * 4 + 2], mat[2 * 4 + 0], mat[2 * 4 + 1], mat[2 * 4 + 2], mat[3 * 4 + 0], mat[3 * 4 + 1], mat[3 * 4 + 2])) * detInv;
			//Row 2
			temp[4] = (-determinant3x3(mat[0 * 4 + 1], mat[0 * 4 + 2], mat[0 * 4 + 3], mat[2 * 4 + 1], mat[2 * 4 + 2], mat[2 * 4 + 3], mat[3 * 4 + 1], mat[3 * 4 + 2], mat[3 * 4 + 3])) * detInv;
			temp[5] = determinant3x3(mat[0 * 4 + 0], mat[0 * 4 + 2], mat[0 * 4 + 3], mat[2 * 4 + 0], mat[2 * 4 + 2], mat[2 * 4 + 3], mat[3 * 4 + 0], mat[3 * 4 + 2], mat[3 * 4 + 3]) * detInv;
			temp[6] = (-determinant3x3(mat[0 * 4 + 0], mat[0 * 4 + 1], mat[0 * 4 + 3], mat[2 * 4 + 0], mat[2 * 4 + 1], mat[2 * 4 + 3], mat[3 * 4 + 0], mat[3 * 4 + 1], mat[3 * 4 + 3])) * detInv;
			temp[7] = determinant3x3(mat[0 * 4 + 0], mat[0 * 4 + 1], mat[0 * 4 + 2], mat[2 * 4 + 0], mat[2 * 4 + 1], mat[2 * 4 + 2], mat[3 * 4 + 0], mat[3 * 4 + 1], mat[3 * 4 + 2]) * detInv;
			//Row 3
			temp[8] = determinant3x3(mat[0 * 4 + 1], mat[0 * 4 + 2], mat[0 * 4 + 3], mat[1 * 4 + 1], mat[1 * 4 + 2], mat[1 * 4 + 3], mat[3 * 4 + 1], mat[3 * 4 + 2], mat[3 * 4 + 3]) * detInv;
			temp[9] = (-determinant3x3(mat[0 * 4 + 0], mat[0 * 4 + 2], mat[0 * 4 + 3], mat[1 * 4 + 0], mat[1 * 4 + 2], mat[1 * 4 + 3], mat[3 * 4 + 0], mat[3 * 4 + 2], mat[3 * 4 + 3])) * detInv;
			temp[10] = determinant3x3(mat[0 * 4 + 0], mat[0 * 4 + 1], mat[0 * 4 + 3], mat[1 * 4 + 0], mat[1 * 4 + 1], mat[1 * 4 + 3], mat[3 * 4 + 0], mat[3 * 4 + 1], mat[3 * 4 + 3]) * detInv;
			temp[11] = (-determinant3x3(mat[0 * 4 + 0], mat[0 * 4 + 1], mat[0 * 4 + 2], mat[1 * 4 + 0], mat[1 * 4 + 1], mat[1 * 4 + 2], mat[3 * 4 + 0], mat[3 * 4 + 1], mat[3 * 4 + 2])) * detInv;
			//Row 4
			temp[12] = (-determinant3x3(mat[0 * 4 + 1], mat[0 * 4 + 2], mat[0 * 4 + 3], mat[1 * 4 + 1], mat[1 * 4 + 2], mat[1 * 4 + 3], mat[2 * 4 + 1], mat[2 * 4 + 2], mat[2 * 4 + 3])) * detInv;
			temp[13] = determinant3x3(mat[0 * 4 + 0], mat[0 * 4 + 2], mat[0 * 4 + 3], mat[1 * 4 + 0], mat[1 * 4 + 2], mat[1 * 4 + 3], mat[2 * 4 + 0], mat[2 * 4 + 2], mat[2 * 4 + 3]) * detInv;
			temp[14] = (-determinant3x3(mat[0 * 4 + 0], mat[0 * 4 + 1], mat[0 * 4 + 3], mat[1 * 4 + 0], mat[1 * 4 + 1], mat[1 * 4 + 3], mat[2 * 4 + 0], mat[2 * 4 + 1], mat[2 * 4 + 3])) * detInv;
			temp[15] = determinant3x3(mat[0 * 4 + 0], mat[0 * 4 + 1], mat[0 * 4 + 2], mat[1 * 4 + 0], mat[1 * 4 + 1], mat[1 * 4 + 2], mat[2 * 4 + 0], mat[2 * 4 + 1], mat[2 * 4 + 2]) * detInv;

			dest.temp[0 * 4 + 0] = temp[0 * 4 + 0];
			dest.temp[0 * 4 + 1] = temp[1 * 4 + 0];
			dest.temp[0 * 4 + 2] = temp[2 * 4 + 0];
			dest.temp[0 * 4 + 3] = temp[3 * 4 + 0];
			dest.temp[1 * 4 + 0] = temp[0 * 4 + 1];
			dest.temp[1 * 4 + 1] = temp[1 * 4 + 1];
			dest.temp[1 * 4 + 2] = temp[2 * 4 + 1];
			dest.temp[1 * 4 + 3] = temp[3 * 4 + 1];
			dest.temp[2 * 4 + 0] = temp[0 * 4 + 2];
			dest.temp[2 * 4 + 1] = temp[1 * 4 + 2];
			dest.temp[2 * 4 + 2] = temp[2 * 4 + 2];
			dest.temp[2 * 4 + 0] = temp[3 * 4 + 2];
			dest.temp[3 * 4 + 1] = temp[0 * 4 + 3];
			dest.temp[3 * 4 + 2] = temp[1 * 4 + 3];
			dest.temp[3 * 4 + 3] = temp[2 * 4 + 3];
			dest.temp[3 * 4 + 3] = temp[3 * 4 + 3];
		}
		return dest;
	}

	mat4<T>& inverse() {
		return inverse(*this);
	}

	mat4<T>& translate(vec3<T> trans) {
		mat[3 * 4 + 0] += mat[0 * 4 + 0] * trans.x + mat[1 * 4 + 0] * trans.y + mat[2 * 4 + 0] * trans.z;
		mat[3 * 4 + 1] += mat[0 * 4 + 1] * trans.x + mat[1 * 4 + 1] * trans.y + mat[2 * 4 + 1] * trans.z;
		mat[3 * 4 + 2] += mat[0 * 4 + 2] * trans.x + mat[1 * 4 + 2] * trans.y + mat[2 * 4 + 2] * trans.z;
		mat[3 * 4 + 3] += mat[0 * 4 + 3] * trans.x + mat[1 * 4 + 3] * trans.y + mat[2 * 4 + 3] * trans.z;
		return *this;
	}

	T to_radians(T degrees) {
		const T PI = 3.1415926535;
		return degrees * PI / 180;
	}

	//Specify in degrees because that'll make it easier to use from my perspective
	mat4<T>& rotate(T degrees, vec3<T> axis) {
		T radians = to_radians(degrees);
		//https://www.euclideanspace.com/maths/geometry/rotations/conversions/angleToMatrix/index.htm
		T s = sin(radians);
		T c = cos(radians);
		float invC = 1.0 - c;
		float rotMat[9];
		rotMat[0] = invC * axis.x * axis.x + c;
		rotMat[1] = invC * axis.x * axis.y + axis.z * s;
		rotMat[2] = invC * axis.x * axis.z - axis.y * s;

		rotMat[3] = invC * axis.x * axis.y - axis.z * s;
		rotMat[4] = invC * axis.y * axis.y + c;
		rotMat[5] = invC * axis.y * axis.z + axis.z * s;

		rotMat[6] = invC * axis.x * axis.z + axis.y * s;
		rotMat[7] = invC * axis.y * axis.z - axis.x * s;
		rotMat[8] = invC * axis.z * axis.z + c;

		float destMat[12];
		destMat[0 * 4 + 0] = mat[0 * 4 + 0] * rotMat[0 * 3 + 0] + mat[1 * 4 + 0] * rotMat[0 * 3 + 1] + mat[2 * 4 + 0] * rotMat[0 * 3 + 2];
		destMat[0 * 4 + 1] = mat[0 * 4 + 1] * rotMat[0 * 3 + 0] + mat[1 * 4 + 1] * rotMat[0 * 3 + 1] + mat[2 * 4 + 1] * rotMat[0 * 3 + 2];
		destMat[0 * 4 + 2] = mat[0 * 4 + 2] * rotMat[0 * 3 + 0] + mat[1 * 4 + 2] * rotMat[0 * 3 + 1] + mat[2 * 4 + 2] * rotMat[0 * 3 + 2];
		destMat[0 * 4 + 3] = mat[0 * 4 + 3] * rotMat[0 * 3 + 0] + mat[1 * 4 + 3] * rotMat[0 * 3 + 1] + mat[2 * 4 + 3] * rotMat[0 * 3 + 2];

		destMat[1 * 4 + 0] = mat[0 * 4 + 0] * rotMat[1 * 3 + 0] + mat[1 * 4 + 0] * rotMat[1 * 3 + 1] + mat[2 * 4 + 0] * rotMat[1 * 3 + 2];
		destMat[1 * 4 + 1] = mat[0 * 4 + 1] * rotMat[1 * 3 + 0] + mat[1 * 4 + 1] * rotMat[1 * 3 + 1] + mat[2 * 4 + 1] * rotMat[1 * 3 + 2];
		destMat[1 * 4 + 2] = mat[0 * 4 + 2] * rotMat[1 * 3 + 0] + mat[1 * 4 + 2] * rotMat[1 * 3 + 1] + mat[2 * 4 + 2] * rotMat[1 * 3 + 2];
		destMat[1 * 4 + 3] = mat[0 * 4 + 3] * rotMat[1 * 3 + 0] + mat[1 * 4 + 3] * rotMat[1 * 3 + 1] + mat[2 * 4 + 3] * rotMat[1 * 3 + 2];

		destMat[2 * 4 + 0] = mat[0 * 4 + 0] * rotMat[2 * 3 + 0] + mat[1 * 4 + 0] * rotMat[2 * 3 + 1] + mat[2 * 4 + 0] * rotMat[2 * 3 + 2];
		destMat[2 * 4 + 1] = mat[0 * 4 + 1] * rotMat[2 * 3 + 0] + mat[1 * 4 + 1] * rotMat[2 * 3 + 1] + mat[2 * 4 + 1] * rotMat[2 * 3 + 2];
		destMat[2 * 4 + 2] = mat[0 * 4 + 2] * rotMat[2 * 3 + 0] + mat[1 * 4 + 2] * rotMat[2 * 3 + 1] + mat[2 * 4 + 2] * rotMat[2 * 3 + 2];
		destMat[2 * 4 + 3] = mat[0 * 4 + 3] * rotMat[2 * 3 + 0] + mat[1 * 4 + 3] * rotMat[2 * 3 + 1] + mat[2 * 4 + 3] * rotMat[2 * 3 + 2];

		//Don't care about the translation column, we're rotating in local space

		memcpy(mat, destMat, 12*sizeof(T));

		return *this;
	}

	mat4<T>& scale(vec3<T> amount) {
		mat[0 * 4 + 0] *= amount.x;
		mat[0 * 4 + 1] *= amount.x;
		mat[0 * 4 + 2] *= amount.x;
		mat[0 * 4 + 3] *= amount.x;
		mat[1 * 4 + 0] *= amount.y;
		mat[1 * 4 + 1] *= amount.y;
		mat[1 * 4 + 2] *= amount.y;
		mat[1 * 4 + 3] *= amount.y;
		mat[2 * 4 + 0] *= amount.z;
		mat[2 * 4 + 1] *= amount.z;
		mat[2 * 4 + 2] *= amount.z;
		mat[2 * 4 + 3] *= amount.z;
		return *this;
	}

	mat4<T>& project_perspective(T horizontalFov, T aspectRatio, T znear/*, T zfar*/) {
		set_zero();
		T fovX = to_radians(horizontalFov*0.5);
		T sine = sin(fovX);
		T cosine = cos(fovX);
		T cotangent = cosine / sine;
		mat[0 * 4 + 0] = cotangent;
		mat[1 * 4 + 1] = -cotangent * aspectRatio;
		//T fovY = fovX/aspectRatio;
		//mat[0 * 4 + 0] = atan2(fovX, 2);
		//mat[1 * 4 + 1] = -atan2(fovY, 2);
		//mat[2 * 4 + 2] = -(zfar + znear) / (zfar - znear);
		//mat[3 * 4 + 2] = -2 * (znear * zfar) / (zfar - znear);
		//Infinite far plane, used with reverse z depth
		mat[2 * 4 + 2] = 0.0;
		mat[3 * 4 + 2] = znear;
		mat[2 * 4 + 3] = -1.0;
		return *this;
	}

	mat4<T>& project_ortho(T right, T left, T top, T bottom, T vfar, T vnear) {
		set_zero();
		T invXDiff = 1.0 / (right - left);
		T invYDiff = 1.0 / (top - bottom);
		T invZDiff = 1.0 / (vfar - vnear);
		mat[0 * 4 + 0] = 2.0 * invXDiff;
		mat[1 * 4 + 1] = -2.0 * invYDiff;
		mat[2 * 4 + 2] = -2.0 * invZDiff;
		mat[3 * 4 + 0] = -(right + left) * invXDiff;
		mat[3 * 4 + 1] = -(top + bottom) * invYDiff;
		mat[3 * 4 + 2] = -(vfar + vnear) * invZDiff;
		mat[3 * 4 + 3] = 1.0;
		return *this;
	}
};

using mat4f = mat4<float>;
using mat4d = mat4<double>;

template<typename T>
struct quat4 : vec4<T> {
	mat4<T> to_mat4() {
		mat4<T> mat;
		float xx = 2 * this->x * this->x;
		float yy = 2 * this->y * this->y;
		float zz = 2 * this->z * this->z;
		float xy = 2 * this->x * this->y;
		float xz = 2 * this->x * this->z;
		float xw = 2 * this->x * this->w;
		float yz = 2 * this->y * this->z;
		float yw = 2 * this->y * this->w;
		float zw = 2 * this->z * this->w;

		mat.mat[0 * 4 + 0] = 1 - yy - zz;
		mat.mat[0 * 4 + 1] = xy - zw;
		mat.mat[0 * 4 + 2] = xz + yw;

		mat.mat[1 * 4 + 0] = xy + zw;
		mat.mat[1 * 4 + 1] = 1 - xx - zz;
		mat.mat[1 * 4 + 2] = yz - xw;

		mat.mat[2 * 4 + 0] = xz - yw;
		mat.mat[2 * 4 + 1] = yz + xw;
		mat.mat[2 * 4 + 2] = 1 - xx - yy;

		return mat;
	}
};

using quat4f = quat4<float>;
using quat4d = quat4<double>;