#pragma once
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <iostream>

#define DM_PI 3.1415926535
#define DM_TWO_PI (DM_PI*2)

template<typename T>
inline T signum(T val) {
	return (val > T{ 0 }) - (val < T{ 0 });
}

template<typename T>
inline T clamp(T val, T min, T max) {
	return std::min(max, std::max(val, min));
}

template<typename T>
inline T clamp01(T val) {
	return clamp(val, T{ 0 }, T{ 1 });
}

template<typename T>
inline T max3(T v0, T v1, T v2) {
	return std::max(v0, std::max(v1, v2));
}

template<typename T>
inline T min3(T v0, T v1, T v2) {
	return std::min(v0, std::min(v1, v2));
}

template<typename T>
inline T median(T v0, T v1, T v2) {
	return std::max(std::min(v0, v1), std::min(std::max(v0, v1), v2));
}

template<typename T>
inline T to_radians(T degrees) {
	constexpr T PI = DM_PI;
	return degrees * (PI / 180);
}

template<typename T>
inline T to_degrees(T radians) {
	constexpr T PI = DM_PI;
	return radians * (180 / PI);
}

template<typename T>
inline T remap(T val, T oldMin, T oldMax, T newMin, T newMax) {
	return ((val - oldMin) / (oldMax - oldMin)) * (newMax - newMin) + newMin;
}

template<typename T>
inline T remap01(T val, T oldMin, T oldMax) {
	return (val - oldMin) / (oldMax - oldMin);
}

template<uint8_t Size, typename T, typename Vec, uint8_t A>
struct swizzle1 {
	T components[Size];
	Vec& operator=(const Vec& other) {
		components[A] = other;
		return components[A];
	}
	Vec& operator+=(const Vec& other) {
		components[A] += other;
		return components[A];
	}
	Vec& operator-=(const Vec& other) {
		components[A] -= other;
		return components[A];
	}
	operator T() {
		return components[A];
	}
	
};

/*template<uint8_t Size, typename T, typename Vec, uint8_t A>
std::ostream& operator<<(std::ostream& out, const swizzle1<Size, T, Vec, A>& vec) {
	out << vec.components[A];
	return out;
}*/

template<uint8_t Size, typename T, typename Vec, uint8_t A, uint8_t B>
struct swizzle2 {
	T components[Size];
	Vec& operator=(const Vec& other) {
		components[A] = other.components[A];
		components[B] = other.components[B];
		return *this;
	}
	operator Vec() {
		return Vec{ components[A], components[B] };
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
		return Vec{ components[A], components[B], components[C] };
	}
};

template<typename T>
class vec2 {
public:
	union {
		T components[2];
		swizzle1<2, T, T, 0> x, r, s;
		swizzle1<2, T, T, 1> y, g, t;
		swizzle2<2, T, vec2, 0, 1> xy, rg, st;
		swizzle2<2, T, vec2, 1, 0> yx, gr, ts;
	};

	vec2() {
		components[0] = 0.0;
		components[1] = 0.0;
	}

	vec2(T x, T y) {
		components[0] = x;
		components[1] = y;
	}
	vec2(T all) {
		components[0] = all;
		components[1] = all;
	}

	T& operator[](const uint32_t idx) {
		return components[idx];
	}

	bool operator==(const vec2<T>& other) {
		return components[0] == other.components[0] && components[1] == other.components[1];
	}

	bool operator!=(const vec2<T>& other) {
		return !operator==(other);
	}

	vec2<T> operator+(const vec2<T>& other) {
		vec2<T> newVec{ *this };
		newVec.components[0] = this->components[0] + other.components[0];
		newVec.components[1] = this->components[1] + other.components[1];
		return newVec;
	}

	vec2<T> operator-(const vec2<T>& other) {
		vec2<T> newVec{ *this };
		newVec.components[0] = this->components[0] - other.components[0];
		newVec.components[1] = this->components[1] - other.components[1];
		return newVec;
	}

	vec2<T> operator*(const vec2<T>& other) {
		vec2<T> newVec{ *this };
		newVec.components[0] = this->components[0] * other.components[0];
		newVec.components[1] = this->components[1] * other.components[1];
		return newVec;
	}

	vec2<T> operator/(const vec2<T>& other) {
		vec2<T> newVec{ *this };
		newVec.components[0] = this->components[0] / other.components[0];
		newVec.components[1] = this->components[1] / other.components[1];
		return newVec;
	}

	vec2<T>& operator+=(const vec2<T>& other) {
		this->components[0] += other.components[0];
		this->components[1] += other.components[1];
		return *this;
	}

	vec2<T>& operator-=(const vec2<T>& other) {
		this->components[0] -= other.components[0];
		this->components[1] -= other.components[1];
		return *this;
	}

	vec2<T>& operator*=(const vec2<T>& other) {
		this->components[0] *= other.components[0];
		this->components[1] *= other.components[1];
		return *this;
	}

	vec2<T>& operator/=(const vec2<T>& other) {
		this->components[0] /= other.components[0];
		this->components[1] /= other.components[1];
		return *this;
	}

	vec2<T> operator+(T other) {
		vec2<T> newVec{ *this };
		newVec.components[0] = this->components[0] + other;
		newVec.components[1] = this->components[1] + other;
		return newVec;
	}

	vec2<T> operator-(T other) {
		vec2<T> newVec{ *this };
		newVec.components[0] = this->components[0] - other;
		newVec.components[1] = this->components[1] - other;
		return newVec;
	}

	vec2<T> operator*(T other) {
		vec2<T> newVec{ *this };
		newVec.components[0] = this->components[0] * other;
		newVec.components[1] = this->components[1] * other;
		return newVec;
	}

	vec2<T> operator/(T other) {
		T invArg = 1.0 / other;
		vec2<T> newVec{ *this };
		newVec.components[0] = this->components[0] * invArg;
		newVec.components[1] = this->components[1] * invArg;
		return newVec;
	}

	vec2<T> operator-() {
		vec2<T> newVec{ *this };
		newVec.components[0] = -this->components[0];
		newVec.components[1] = -this->components[1];
		return newVec;
	}

	vec2<T> lerp(const vec2<T>& other, T t) {
		return vec2<T>{components[0] + (other.components[0] - components[0]) * t, components[1] + (other.components[1] - components[1]) * t};
	}

	vec2<T>& normalize() {
		T len = sqrtf(components[0] * components[0] + components[1] * components[1]);
		constexpr T epsilon = 0.000001;
		if (len < epsilon) {
			components[0] = 0;
			components[1] = 0;
			return *this;
		}
		T invLen = 1.0 / len;
		components[0] *= invLen;
		components[1] *= invLen;
		return *this;
	}

	T cross(const vec2<T>& other) {
		return components[0] * other.components[1] - components[1] * other.components[0];
	}

	T dot(const vec2<T>& other) {
		return other.components[0] * components[0] + other.components[1] * components[1];
	}

	T length() {
		return sqrtf(components[0] * components[0] + components[1] * components[1]);
	}

	T sum_components() {
		return components[0] + components[1];
	}

};

template<typename T>
std::ostream& operator<<(std::ostream& out, const vec2<T>& vec) {
	out << "vec2: " << vec.components[0] << " " << vec.components[1];
	return out;
}

template<typename T>
inline vec2<T> normalize(vec2<T> vec) {
	return vec2<T>(vec).normalize();
}

template<typename T>
inline T length_sq(vec2<T> vec) {
	return vec.x * vec.x + vec.y * vec.y;
}

template<typename T>
inline T length(vec2<T> vec) {
	return vec.length();
}
	
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

	vec3<T> operator+(const vec3<T>& other) {
		return vec3<T>{ components[0] + other.components[0], components[1] + other.components[1], components[2] + other.components[2] };
	}

	vec3<T> operator-(const vec3<T>& other) {
		return vec3<T>{ components[0] - other.components[0], components[1] - other.components[1], components[2] - other.components[2] };
	}

	vec3<T> operator-() {
		return vec3<T>{ -components[0], -components[1], -components[2] };
	}
	
	vec3<T>& operator+=(const vec3<T>& other) {
		components[0] += other.components[0];
		components[1] += other.components[1];
		components[2] += other.components[2];
		return *this;
	}

	vec3<T>& operator-=(const vec3<T>& other) {
		components[0] -= other.components[0];
		components[1] -= other.components[1];
		components[2] -= other.components[2];
		return *this;
	}

	vec3<T> operator*(const vec3<T>& other) {
		return vec3<T>{ components[0] * other.components[0], components[1] * other.components[1], components[2] * other.components[2] };
	}

	vec3<T> operator*(const T other) {
		return vec3<T>{ components[0] * other, components[1] * other, components[2] * other };
	}

	void operator*=(const vec3<T>& other) {
		components[0] *= other.components[0];
		components[1] *= other.components[1];
		components[2] *= other.components[2];
	}

	void operator*=(const T other) {
		components[0] *= other;
		components[1] *= other;
		components[2] *= other;
	}

	vec3<T> lerp(const vec3<T>& other, T t) {
		return vec3<T>{components[0] + (other.components[0] - components[0]) * t, components[1] + (other.components[1] - components[1]) * t, components[2] + (other.components[2] - components[2]) * t};
	}

	vec3<T>& normalize() {
		T len = sqrtf(components[0] * components[0] + components[1] * components[1] + components[2] * components[2]);
		constexpr T epsilon = 0.000001;
		if (len < epsilon) {
			components[0] = 0;
			components[1] = 0;
			components[2] = 0;
			return *this;
		}
		T invLen = 1.0 / len;
		components[0] *= invLen;
		components[1] *= invLen;
		components[2] *= invLen;
		return *this;
	}

	vec3<T> cross(const vec3<T>& other) {
		return vec3<T>{ components[1] * other.components[2] - components[2] * other.components[1], components[2] * other.components[0] - components[0] * other.components[2], components[0] * other.components[1] - components[1] * other.components[0] };
	}

	T dot(const vec3<T>& other) {
		return other.components[0] * components[0] + other.components[1] * components[1] + other.components[2] * components[2];
	}

	T length() {
		return sqrtf(components[0] * components[0] + components[1] * components[1] + components[2] * components[2]);
	}

	T sum_components() {
		return components[0] + components[1] + components[2];
	}
};
using vec3f = vec3<float>;

template<typename T>
std::ostream& operator<<(std::ostream& out, const vec3<T>& vec) {
	out << "vec3: " << vec.components[0] << " " << vec.components[1] << " " << vec.components[2];
	return out;
}

template<typename T>
inline vec3<T> normalize(vec3<T> vec) {
	return vec3<T>(vec).normalize();
}

template<typename T>
inline T length_sq(vec3<T> vec) {
	return vec.x * vec.x + vec.y * vec.y + vec.z * vec.z;
}

template<typename T>
inline T length(vec3<T> vec) {
	return vec.length();
}

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
	vec4ui8(float color) {
		vec4<uint8_t>(color, color, color, color);
	}
};

template<typename T>
std::ostream& operator<<(std::ostream& out, const vec4<T>& vec) {
	out << "vec4: " << vec.components[0] << " " << vec.components[1] << " " << vec.components[2] << " " << vec.components[3];
	return out;
}

template<typename T>
struct mat2 {
	T mat[4];

	mat2() {
		set_identity();
	}

	mat2<T>& set_identity() {
		mat[0] = 1;
		mat[1] = 0;
		mat[2] = 0;
		mat[3] = 1;
		return *this;
	}

	mat2<T>& set_zero() {
		memset(mat, 0, 4*sizeof(T));
		return *this;
	}

	mat2<T>& set(mat2<T> other) {
		memcpy(mat, other.mat, 4*sizeof(T));
		return *this;
	}

	mat2<T>& rotate(T angle) {
		T angleRadians = to_radians(angle);
		T s = sin(angleRadians);
		T c = cos(angleRadians);
		T result[4];
		result[0] = mat[0] * c + mat[2] * s;
		result[1] = mat[1] * c + mat[3] * s;
		result[2] = mat[0] * -s + mat[2] * c;
		result[3] = mat[1] * -s + mat[3] * c;
		memcpy(mat, result, 4*sizeof(T));
		return *this;
	}

	mat2<T>& mul(mat2<T> other) {
		T result[4];
		result[0] = mat[0] * other.mat[0] + mat[2] * other.mat[1];
		result[1] = mat[1] * other.mat[0] + mat[3] * other.mat[1];
		result[2] = mat[0] * other.mat[2] + mat[2] * other.mat[3];
		result[3] = mat[1] * other.mat[2] + mat[3] * other.mat[3];
		memcpy(mat, result, 4*sizeof(T));
		return *this;
	}

	mat2<T> operator*(const mat2<T>& other) {
		mat2<T> result{*this};
		return result.mul(other);
	}

	mat2<T>& operator*=(const mat2<T>& other) {
		return mul(other);
	}

	T determinant() {
		return mat[0] * mat[3] - mat[1] * mat[2];
	}

	mat2<T>& inverse() {
		T invDet = 1.0 / determinant();
		T result[4];
		result[0] = mat[3] * invDet;
		result[1] = -mat[1] * invDet;
		result[2] = -mat[2] * invDet;
		result[3] = mat[0] * invDet;
		memcpy(mat, result, 4*sizeof(T));
		return *this;
	}

	mat2<T>& transpose() {
		T swap = mat[2];
		mat[2] = mat[1];
		mat[1] = swap;
		return *this;
	}

	vec2<T> transform(const vec2<T>& vec) {
		T x = mat[0] * vec.components[0] + mat[2] * vec.components[1];
		T y = mat[1] * vec.components[0] + mat[3] * vec.components[1];
		return vec2<T>{x, y};
	}

	vec2<T> operator*(const vec2<T>& vec) {
		return transform(vec);
	}

	vec2<T>& operator*=(const vec2<T>& vec) {
		T x = mat[0] * vec.x + mat[2] * vec.y;
		T y = mat[1] * vec.x + mat[3] * vec.y;
		vec.x = x;
		vec.y = y;
		return vec;
	}
};

using mat2f = mat2<float>;
using mat2d = mat2<double>;

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
			temp[2] = determinant3x3(mat[1 * 4 + 0], mat[1 * 4 + 1], mat[1 * 4 + 3], mat[2 * 4 + 0], mat[2 * 4 + 1], mat[2 * 4 + 3], mat[3 * 4 + 0], mat[3 * 4 + 1], mat[3 * 4 + 3]) * detInv;
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

			dest.mat[0 * 4 + 0] = temp[0 * 4 + 0];
			dest.mat[0 * 4 + 1] = temp[1 * 4 + 0];
			dest.mat[0 * 4 + 2] = temp[2 * 4 + 0];
			dest.mat[0 * 4 + 3] = temp[3 * 4 + 0];
			dest.mat[1 * 4 + 0] = temp[0 * 4 + 1];
			dest.mat[1 * 4 + 1] = temp[1 * 4 + 1];
			dest.mat[1 * 4 + 2] = temp[2 * 4 + 1];
			dest.mat[1 * 4 + 3] = temp[3 * 4 + 1];
			dest.mat[2 * 4 + 0] = temp[0 * 4 + 2];
			dest.mat[2 * 4 + 1] = temp[1 * 4 + 2];
			dest.mat[2 * 4 + 2] = temp[2 * 4 + 2];
			dest.mat[2 * 4 + 3] = temp[3 * 4 + 2];
			dest.mat[3 * 4 + 0] = temp[0 * 4 + 3];
			dest.mat[3 * 4 + 1] = temp[1 * 4 + 3];
			dest.mat[3 * 4 + 2] = temp[2 * 4 + 3];
			dest.mat[3 * 4 + 3] = temp[3 * 4 + 3];
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

	mat4<T>& translate_global(vec3<T> trans) {
		mat[3 * 4 + 0] += trans.x;
		mat[3 * 4 + 1] += trans.y;
		mat[3 * 4 + 2] += trans.z;
		return *this;
	}

	mat4<T> set_translation(vec3<T> trans) {
		mat[3 * 4 + 0] = trans.x;
		mat[3 * 4 + 1] = trans.y;
		mat[3 * 4 + 2] = trans.z;
		return *this;
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
		rotMat[5] = invC * axis.y * axis.z + axis.x * s;

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

	vec4<T> transform(vec4<T>& vec) {
		vec4<T> result;
		result.x = mat[0 * 4 + 0] * vec.x + mat[1 * 4 + 0] * vec.y + mat[2 * 4 + 0] * vec.z + mat[3 * 4 + 0] * vec.w;
		result.y = mat[0 * 4 + 1] * vec.x + mat[1 * 4 + 1] * vec.y + mat[2 * 4 + 1] * vec.z + mat[3 * 4 + 1] * vec.w;
		result.z = mat[0 * 4 + 2] * vec.x + mat[1 * 4 + 2] * vec.y + mat[2 * 4 + 2] * vec.z + mat[3 * 4 + 2] * vec.w;
		result.w = mat[0 * 4 + 3] * vec.x + mat[1 * 4 + 3] * vec.y + mat[2 * 4 + 3] * vec.z + mat[3 * 4 + 3] * vec.w;
		return result;
	}

	vec4<T> operator*(vec4<T> vec) {
		return transform(vec);
	}

	mat4<T> operator*(mat4<T>& mat) {
		mat4<T> dest;
		mul(mat, dest);
		return dest;
	}

	void operator*=(mat4<T>& mat) {
		mul(mat, *this);
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

	mat4<T>& project_ortho(T right, T left, T top, T bottom, T zfar, T znear) {
		set_zero();
		T invXDiff = 1.0 / (right - left);
		T invYDiff = 1.0 / (top - bottom);
		T invZDiff = 1.0 / (znear - zfar);
		mat[0 * 4 + 0] = 2.0 * invXDiff;
		mat[1 * 4 + 1] = 2.0 * invYDiff;
		mat[2 * 4 + 2] = invZDiff;
		mat[3 * 4 + 0] = -(right + left) * invXDiff;
		mat[3 * 4 + 1] = -(top + bottom) * invYDiff;
		mat[3 * 4 + 2] = znear;
		mat[3 * 4 + 3] = 1.0;
		return *this;
	}
};

using mat4f = mat4<float>;
using mat4d = mat4<double>;

template<typename T>
std::ostream& operator<<(std::ostream& out, mat4<T>& mat) {
	out << "mat4: \n" << mat[0][0] << " " << mat[0][1] << " " << mat[0][2] << " " << mat[0][3] << "\n";
	out << mat[1][0] << " " << mat[1][1] << " " << mat[1][2] << " " << mat[1][3] << "\n";
	out << mat[2][0] << " " << mat[2][1] << " " << mat[2][2] << " " << mat[2][3] << "\n";
	out << mat[3][0] << " " << mat[3][1] << " " << mat[3][2] << " " << mat[3][3];
	return out;
}

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

namespace dm {
	enum Axis {
		X = 0,
		Y = 1,
		Z = 2,
		NONE = 3
	};

	enum class Direction {
		DOWN = 0,
		UP = 1,
		NORTH = 2,
		SOUTH = 3,
		EAST = 4,
		WEST = 5,
		NONE = 6
	};
	inline const char* directionNames[7]{ "DOWN", "UP", "NORTH", "SOURTH", "EAST", "WEST", "NONE" };
	inline vec3f directionOffsets[7]{ {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0 , 0}, {0, 0, 0} };
	inline Axis directionAxis[7]{ Axis::Y, Axis::Y, Axis::Z, Axis::Z, Axis::X, Axis::X, Axis::NONE };
	inline Direction directionOpposites[7]{ Direction::UP, Direction::DOWN, Direction::SOUTH, Direction::NORTH, Direction::WEST, Direction::EAST, Direction::NONE };

	enum class Direction2D {
		DOWN = 0,
		UP = 1,
		LEFT = 2,
		RIGHT = 3,
		NONE = 4
	};
	inline const char* direction2DNames[5]{ "DOWN", "UP", "LEFT", "RIGHT", "NONE" };
	inline vec2f direction2DOffsets[5]{ {0, -1}, {0, 1}, {-1, 0}, {1, 0}, {0, 0} };
	inline Axis direction2DAxis[5]{ Axis::Y, Axis::Y, Axis::X, Axis::X, Axis::NONE };
	inline Direction2D direction2DOpposites[5]{ Direction2D::UP, Direction2D::DOWN, Direction2D::RIGHT, Direction2D::LEFT, Direction2D::NONE };
}

template<typename T>
struct AxisAlignedBB2D {
	T minX, minY, maxX, maxY;

	inline bool contains(AxisAlignedBB2D<T>& other) {
		return (minX <= other.maxX) && (maxX >= other.minX) && (minY <= other.minY) && (maxY >= other.minY);
	}

	inline bool intersects(AxisAlignedBB2D<T>& other) {
		return ((minX <= other.maxX && minX >= other.minX) || (maxX <= other.maxX && maxX >= other.minX)) &&
			((minY <= other.maxY && minY >= other.minY) || (maxY <= other.maxY && maxY >= other.minY));
	}

	inline AxisAlignedBB2D<T> box_intersection(AxisAlignedBB2D<T>& other) {
		return AxisAlignedBB2D<T>{ std::max(minX, other.minX), std::max(minY, other.minY), std::min(maxX, other.maxX), std::min(maxY, other.maxY) };
	}

	inline AxisAlignedBB2D<T> box_union(AxisAlignedBB2D<T>& other) {
		return AxisAlignedBB2D<T>{ std::min(minX, other.minX), std::min(minY, other.minY), std::max(maxX, other.maxX), std::max(maxY, other.maxY) };
	}

	inline AxisAlignedBB2D<T> expand(T expansion) {
		return AxisAlignedBB2D<T>{ minX - expansion, minY - expansion, maxX + expansion, maxY + expansion };
	}

	inline T distance(vec2<T> point) {
		return std::max(std::min(std::min(minX - point.x, point.x - maxX), std::min(minY - point.y, point.y - maxY)), 0.0);
	}

	inline bool contains(vec2<T> point) {
		return (point.x >= minX) && (point.x <= maxX) && (point.y >= minY) && (point.y <= maxY);
	}

	static constexpr uint32_t IDX_LEFT = 0;
	static constexpr uint32_t IDX_DOWN = 1;
	static constexpr uint32_t IDX_RIGHT = 2;
	static constexpr uint32_t IDX_UP = 3;
	static inline uint32_t directionIndices[4]{ 1, 3, 0, 2 };
	static inline uint32_t indexOpposites[4]{ 2, 3, 0, 1 };

	T& operator[](uint32_t index) {
		switch (index) {
		case 0:
			return minX;
		case 1:
			return minY;
		case 2:
			return maxX;
		case 3:
			return maxY;
		default:
			return minX;
		}
	}

	T width(dm::Axis axis) {
		if (axis == dm::Axis::X) {
			return maxX - minX;
		} else {
			return maxY - minY;
		}
	}
};

template<typename T>
struct AxisAlignedBB3D {
	T minX, minY, minZ, maxX, maxY, maxZ;
};

using AxisAlignedBB2Df = AxisAlignedBB2D<float>;
using AxisAlignedBB3Df = AxisAlignedBB3D<float>;