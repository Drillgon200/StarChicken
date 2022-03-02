#include <string>
#include <vector>
#include <exception>
#include "Util.h"
#include <assert.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <functional>
#include <unordered_map>
#include "DrillMath.h"
#include "FontRenderer.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define __STDC_LIB_EXT1__
#include <stb_image_write.h>

namespace util {
	namespace msdf {

#pragma pack(push, 1)
		struct RGBA8 {
			uint8_t r;
			uint8_t g;
			uint8_t b;
			uint8_t a;

			RGBA8() : r{ 0 }, g{ 0 }, b{ 0 }, a{ 0 } {
			}
			RGBA8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : r{ r }, g{ g }, b{ b }, a{ a } {
			}
			RGBA8(uint8_t r, uint8_t g, uint8_t b) : r{ r }, g{ g }, b{ b }, a{ 0 } {
			}
			RGBA8(uint8_t value) : r{ value }, g{ value }, b{ value }, a{ value } {
			}

			bool operator==(RGBA8 other) {
				return r == other.r && g == other.g && b == other.b && a == other.a;
			}

			/*RGBA8 operator*(RGBA8 other) {
				return RGBA8{ r * other.r, g * other.g, b * other.b, a * other.a };
			}

			RGBA8 operator+(RGBA8 other) {
				return RGBA8{ r + other.r, g + other.g, b + other.b, a + other.a };
			}*/

		};
#pragma pack(pop)

		struct ShapeSegment;

		struct SignedDistance {
			ShapeSegment* segment;
			float t;
			float distance;
			float orthogonality;

			SignedDistance(ShapeSegment* seg, float distance, float ortho) : segment{ seg }, distance{ distance }, orthogonality{ ortho }, t{ 0.0F } {
			}

			bool operator>(SignedDistance other) {
				return distance > other.distance || (distance == other.distance && orthogonality < other.orthogonality);
			}

			bool operator<(SignedDistance other) {
				return distance < other.distance || (distance == other.distance && orthogonality > other.orthogonality);
			}

			SignedDistance abs() {
				SignedDistance dist{ *this };
				dist.distance = std::fabs(distance);
				return dist;
			}
		};

		

		struct ShapeSegment {
			RGBA8 color[2] = { { 0 }, { 0 } };
			uint32_t colorCount = 1;

			virtual SignedDistance distance(vec2f pos) = 0;
			virtual float pseudo_distance(vec2f pos, float t) = 0;
			virtual vec2f eval(float t) = 0;
			virtual vec2f direction(float t) = 0;
			virtual void print() = 0;

			RGBA8 get_color(float t) {
				if (colorCount > 1 && t > 0.5F) {
					return color[1];
				} else {
					return color[0];
				}
			}

			static SignedDistance numerical_distance_solve(vec2f pos, ShapeSegment* producer, float eps) {
				constexpr uint32_t NUM_ITERATIONS = 32;

				//Check a bunch of points along the curve, find the closest one.
				SignedDistance minDist{ producer, FLT_MAX, 0 };
				float iteration = 0;
				for (uint32_t i = 0; i < NUM_ITERATIONS; i++) {
					float iN = static_cast<float>(i) / (NUM_ITERATIONS - 1);
					vec2f currentPos = producer->eval(iN);
					float currentDist = length_sq(currentPos - pos);
					if (currentDist < minDist.distance) {
						minDist.distance = currentDist;
						iteration = iN;
					}
				}
				//Refine that point to within an acceptable error. Set a min and max boundary where the actual closest could be (otherwise it would have been another point)
				float min = std::max(0.0F, iteration - (1.0F / NUM_ITERATIONS));
				float max = std::min(1.0F, iteration + (1.0F / NUM_ITERATIONS));
				float med = 0;
				//Simply binary search. Loop until the search area is within acceptable error.
				while ((max - min) > eps) {
					med = (max + min) * 0.5F;
					if (length_sq(producer->eval(std::max(0.0F, med - eps)) - pos) < length_sq(producer->eval(std::min(1.0F, med + eps)) - pos)) {
						max = med;
					} else {
						min = med;
					}
				}
				//Make sure it's exactly one or zero of the min or max never changed from that. That way I don't have to deal with sligtly different floats.
				if (max == 1) {
					med = 1;
				} else if (min == 0) {
					med = 0;
				}
				vec2 closest = producer->eval(med);
				vec2 closestToPos = normalize(pos - closest);
				vec2 closestDir = producer->direction(med);
				float cross = closestDir.cross(closestToPos);
				minDist.distance = length(closest - pos);
				minDist.distance *= signum(cross);
				minDist.orthogonality = fabs(cross);
				minDist.t = med;
				return minDist;
			}

			//Distance to a line but clamp the min projection to 0.
			static float distance_to_ray(vec2f pos, vec2f rayStart, vec2f rayDir) {
				vec2 toPos = pos - rayStart;
				float proj = std::max(0.0F, toPos.dot(rayDir));
				vec2f pointOnRay = rayStart + rayDir * proj;
				toPos = pos - pointOnRay;
				float dist = length(pos - pointOnRay);
				float cross = rayDir.cross(toPos);
				return dist * signum(cross);
			}

			static float bezier_pseudo_distance(ShapeSegment* producer, vec2f pos, float t) {
				vec2f closest = producer->eval(t);
				vec2 closestToPos = normalize(pos - closest);
				vec2 closestDir = producer->direction(t);
				float cross = closestDir.cross(closestToPos);
				float distance = length(closest - pos) * signum(cross);

				float rayDist = distance_to_ray(pos, producer->eval(1.0F), producer->direction(1.0F));
				if (fabs(rayDist) < fabs(distance)) {
					distance = rayDist;
				}
				rayDist = distance_to_ray(pos, producer->eval(0.0F), -producer->direction(0.0F));
				if (fabs(rayDist) < fabs(distance)) {
					//Negate here because the ray is going the opposite direction, so winding order will also be opposite, making the sign opposite.
					distance = -rayDist;
				}
				return distance;
			}
		};

		struct LinearShapeSegment : ShapeSegment {
			vec2f positions[2];
			LinearShapeSegment(vec2f start, vec2f end) {
				positions[0] = start;
				positions[1] = end;
				if (length_sq(start - end) < 1.0F) {
					std::cout << "wrong";
				}
			}

			void print() override {
				std::cout << " linear:" << positions[0].x << " " << positions[0].y << ", " << positions[1].x << " " << positions[1].y;
			}

			SignedDistance distance(vec2f pos) override {
				vec2f line = positions[1] - positions[0];
				float lineLenSq = length_sq(line);
				vec2f toPos = pos - positions[0];
				float proj = clamp01(line.dot(toPos)/lineLenSq);
				vec2f pointOnLine = positions[0] + line * proj;
				toPos = pos - pointOnLine;
				float dist = length(pos - pointOnLine);
				float cross = normalize(line).cross(normalize(toPos));
				dist *= signum(cross);
				SignedDistance sdist = SignedDistance{ this, dist, fabs(cross) };
				sdist.t = proj;
				return sdist;
			}

			float pseudo_distance(vec2f pos, float t) override {
				vec2f line = positions[1] - positions[0];
				float lineLenSq = length_sq(line);
				vec2f toPos = pos - positions[0];
				float proj = line.dot(toPos) / lineLenSq;
				vec2f pointOnLine = positions[0] + line * proj;
				toPos = pos - pointOnLine;
				float dist = length(pos - pointOnLine);
				float cross = normalize(line).cross(normalize(toPos));
				dist *= signum(cross);
				return dist;
			}

			vec2f eval(float t) override {
				return positions[0] + (positions[1] - positions[0]) * t;
			}

			vec2f direction(float t) override {
				return normalize(positions[1] - positions[0]);
			}
		};

		struct QuadraticShapeSegment : ShapeSegment {
			vec2f positions[3];
			QuadraticShapeSegment(vec2f start, vec2f handle, vec2f end) {
				positions[0] = start;
				positions[1] = handle;
				positions[2] = end;
				if (length_sq(start - end) < 1.0F) {
					std::cout << "wrong";
				}
				if (length_sq(start - handle) < 1.0F) {
					std::cout << "wrong";
				}
				if (length_sq(handle - end) < 1.0F) {
					std::cout << "wrong";
				}
			}

			void print() override {
				std::cout << " quadratic:" << positions[0].x << " " << positions[0].y << ", " << positions[1].x << " " << positions[1].y << ", " << positions[2].x << " " << positions[2].y;
			}

			SignedDistance distance(vec2f pos) override {
				return numerical_distance_solve(pos, this, 0.001F);
			}

			float pseudo_distance(vec2f pos, float t) override {
				return bezier_pseudo_distance(this, pos, t);
			}

			vec2f eval(float t) override {
				if (t == 0.0F) {
					return positions[0];
				} else if (t == 1.0F) {
					return positions[2];
				} else {
					vec2f p1 = positions[0].lerp(positions[1], t);
					vec2f p2 = positions[1].lerp(positions[2], t);

					return p1.lerp(p2, t);
				}
			}

			vec2f direction(float t) override {
				if (t == 0) {
					return normalize(positions[0] - positions[1]);
				} else if (t == 1) {
					return normalize(positions[2] - positions[1]);
				} else {
					vec2f p1 = positions[0].lerp(positions[1], t);
					vec2f p2 = positions[1].lerp(positions[2], t);

					return normalize(p2 - p1);
				}
			}
		};

		struct CubicShapeSegment : ShapeSegment {
			vec2f positions[4];
			CubicShapeSegment(vec2f start, vec2f handle1, vec2f handle2, vec2f end) {
				positions[0] = start;
				positions[1] = handle1;
				positions[2] = handle2;
				positions[3] = end;
				if (length_sq(start-end) < 1.0F) {
					std::cout << "wrong";
				}
				if (length_sq(start - handle2) < 1.0F) {
					std::cout << "wrong";
				}
				if (length_sq(end - handle1) < 1.0F) {
					std::cout << "wrong";
				}
				if (length_sq(start - end) < 1.0F) {
					std::cout << "wrong";
				}
				if (length_sq(start - handle1) < 1.0F) {
					std::cout << "wrong";
				}
				if (length_sq(handle1 - handle2) < 1.0F) {
					std::cout << "wrong";
				}
				if (length_sq(handle2 - end) < 1.0F) {
					std::cout << "wrong";
				}
			}

			void print() override {
				std::cout << " cubic:" << positions[0].x << " " << positions[0].y << ", " << positions[1].x << " " << positions[1].y << ", " << positions[2].x << " " << positions[2].y << ", " << positions[3].x << " " << positions[3].y;
			}

			SignedDistance distance(vec2f pos) override {
				return numerical_distance_solve(pos, this, 0.001F);
			}

			float pseudo_distance(vec2f pos, float t) override {
				return bezier_pseudo_distance(this, pos, t);
			}

			vec2f eval(float t) override {
				if (t == 0.0F) {
					return positions[0];
				} else if (t == 1.0F) {
					return positions[3];
				} else {
					vec2f p1 = positions[0].lerp(positions[1], t);
					vec2f p2 = positions[1].lerp(positions[2], t);
					vec2f p3 = positions[2].lerp(positions[3], t);

					vec2f p4 = p1.lerp(p2, t);
					vec2f p5 = p2.lerp(p3, t);

					return p4.lerp(p5, t);
				}
			}

			vec2f direction(float t) override {
				if (t == 0) {
					return normalize(positions[1] - positions[0]);
				} else if (t == 1) {
					return normalize(positions[3] - positions[2]);
				} else {
					vec2f p1 = positions[0].lerp(positions[1], t);
					vec2f p2 = positions[1].lerp(positions[2], t);
					vec2f p3 = positions[2].lerp(positions[3], t);

					vec2f p4 = p1.lerp(p2, t);
					vec2f p5 = p2.lerp(p3, t);

					return normalize(p5 - p4);
				}
			}
		};

		struct ShapePath {
			std::vector<ShapeSegment*> segments;

			SignedDistance distance(vec2f point) {
				SignedDistance min{ nullptr, FLT_MAX, -FLT_MAX };
				for (size_t i = 0; i < segments.size(); i++) {
					SignedDistance pDist = segments[i]->distance(point);
					if (pDist.abs() < min.abs()) {
						min = pDist;
					}
				}
				return min;
			}
		};

		struct Shape {
			std::vector<ShapePath*> paths;

			SignedDistance distance(vec2f point) {
				SignedDistance min{ nullptr, FLT_MAX, -FLT_MAX };
				for (size_t i = 0; i < paths.size(); i++) {
					SignedDistance pDist = paths[i]->distance(point);
					if (pDist.abs() < min.abs()) {
						min = pDist;
					}
				}
				return min;
			}
		};

		char* find_in_str(char* start, const char* str, const char* str2 = nullptr) {
			size_t len = strlen(str);
			size_t len2 = str2 ? strlen(str2) : 0;
			while (strncmp(start, str, len) && ((str2 == nullptr) || strncmp(start, str2, len2))) {
				if (strncmp(start, "</svg>", 6) == 0) {
					return nullptr;
				}
				++start;
			}
			return start;
		}

		void read_cmd_args(std::stringstream& str, std::vector<float>& args) {
			float num{NAN};
			while (str >> num) {
				args.push_back(num);
			}
			str.clear();
		}

		vec2f mat_vec2_transform(mat4f& matrix, vec2f vec) {
			vec4f transform = vec4f(vec.x, vec.y, 0, 1);
			return matrix.transform(transform).xy;
		}

		float vec2_angle(vec2f a, vec2f b) {
			float sign = signum(a.cross(b));
			return sign * acosf(a.dot(b)/(a.length() * b.length()));
		}

		//https://www.w3.org/TR/SVG11/implnote.html#ArcConversionEndpointToCenter
		void endpoint_arc_to_center_arc(vec2f& currentPos, float& xRadius, float& yRadius, float xAxisRotation, bool largeArcFlag, bool sweepFlag, vec2f& nextPos, vec2f* center, float* angleStart, float* angleRange) {
			mat2f matrix;
			matrix.set_identity().rotate(xAxisRotation);
			mat2f matrixTransposed;
			matrixTransposed.set(matrix).transpose();
			vec2f midPos = matrix.transform((currentPos - nextPos) * 0.5);
			float sign = (largeArcFlag == sweepFlag) ? -1.0F : 1.0F;
			float xRadSq = xRadius * xRadius;
			float yRadSq = yRadius * yRadius;
			float midXSq = midPos.x * midPos.x;
			float midYSq = midPos.y * midPos.y;
			float alpha = (midXSq / xRadSq + midYSq / yRadSq);
			if (alpha > 1) {
				float sqrtAlpha = sqrtf(alpha);
				xRadius = xRadius * sqrtAlpha;
				yRadius = yRadius * sqrtAlpha;
			}
			vec2f cDerivative = vec2f{ (xRadius * midPos.y)/yRadius, (-yRadius * midPos.x)/xRadius } * (sign * std::sqrtf((xRadSq * yRadSq - xRadSq * midYSq - yRadSq * midXSq)/(xRadSq * midYSq + yRadSq * midXSq)));
			*center = matrixTransposed * cDerivative + (currentPos + nextPos) * 0.5F;

			*angleStart = vec2_angle(vec2f{ 1, 0 }, vec2f{ (midPos.x-cDerivative.x)/xRadius, (midPos.y-cDerivative.y)/yRadius });
			if (sweepFlag && *angleStart < 0) {
				*angleStart += DM_TWO_PI;
			} else if (!sweepFlag && *angleStart > 0) {
				*angleStart -= DM_TWO_PI;
			}
			*angleRange = fmodf(vec2_angle(vec2f{ (midPos.x - cDerivative.x) / xRadius, (midPos.y - cDerivative.y) / yRadius }, vec2f{ (-midPos.x - cDerivative.x) / xRadius, (-midPos.y - cDerivative.y) / yRadius }), DM_TWO_PI);
		}

		void create_arc(vec2f& currentPos, float xRadius, float yRadius, float xAxisRotation, bool largeArcFlag, bool sweepFlag, vec2f& nextPos, std::vector<ShapeSegment*>& segments) {
			if (xRadius == 0 || yRadius == 0) {
				segments.push_back(new LinearShapeSegment(currentPos, nextPos));
				currentPos = nextPos;
				return;
			}
			xRadius = fabs(xRadius);
			yRadius = fabs(yRadius);
			
			vec2f center;
			float angleStart;
			float angleRange;
			endpoint_arc_to_center_arc(currentPos, xRadius, yRadius, xAxisRotation, largeArcFlag, sweepFlag, nextPos, &center, &angleStart, &angleRange);

			mat4f transform;
			uint32_t subdivisions = static_cast<uint32_t>(ceil(fabs(angleRange)/(DM_PI*0.5)));
			float subdivRange = angleRange/static_cast<float>(subdivisions);
			float sinAngleRange = sin(subdivRange);
			float cosAngleRange = cos(subdivRange);
			float tanQuarterAngleRange = tan(subdivRange * 0.25F);
			constexpr float fourthirds = 4.0F / 3.0F;
			for (uint32_t i = 0; i < subdivisions; i++) {
				float angle = xAxisRotation - to_degrees(angleStart + subdivRange*i);
				transform.set_identity().translate({ center.x, center.y, 0 }).rotate(-angle, { 0, 0, 1 }).scale({ xRadius, yRadius, 1 });

				vec2f bStart;
				vec2f bHandle1 = (transform * vec4f{ 1, fourthirds * tanQuarterAngleRange, 0, 1 }).xy;
				vec2f bHandle2 = (transform * vec4f{ cosAngleRange + fourthirds * tanQuarterAngleRange * sinAngleRange, sinAngleRange - fourthirds * tanQuarterAngleRange * cosAngleRange, 0, 1 }).xy;
				vec2f bEnd;
				//Make sure these points are exact if they are the start or end
				if (i == 0) {
					//bStart = currentPos;
					bStart = (transform * vec4f{ 1, 0, 0, 1 }).xy;
				} else {
					bStart = (transform * vec4f{ 1, 0, 0, 1 }).xy;
				}
				if (i == (subdivisions - 1)) {
					//bEnd = nextPos;
					bEnd = (transform * vec4f{ cosAngleRange, sinAngleRange, 0, 1 }).xy;
				} else {
					bEnd = (transform * vec4f{ cosAngleRange, sinAngleRange, 0, 1 }).xy;
				}
				segments.push_back(new CubicShapeSegment(bStart, bHandle1, bHandle2, bEnd));
			}
		}

		Shape* parse_shape(char* layer) {
			Shape* shape = new Shape{};
			ShapePath* path = nullptr;
			char* found = find_in_str(layer, "d=\"M", "d=\"m");
			found += 3;
			size_t length = 0;
			while (*(found + length) != '"') {
				++length;
			}
			char* insns = new char[length + 1];
			insns[length] = '\0';
			for (size_t i = 0; i < length; i++) {
				insns[i] = found[i];
				if (insns[i] == ',') {
					insns[i] = ' ';
				}
			}
			std::stringstream insn_stream{ insns };
			
			std::vector<float> args{};
			vec2f currentPos{ 0 };
			char insn{ '\0' };
			while (true) {
				if (!(insn_stream >> insn)) {
					break;
				}
				if (insn == ' ') {
					continue;
				}
				bool relative = false;
				if (insn >= 'a' && insn <= 'z') {
					relative = true;
					insn -= ('a' - 'A');
				}
				args.clear();
				read_cmd_args(insn_stream, args);
				switch (insn) {
				case 'M':
				{
					if (path) {
						shape->paths.push_back(path);
					}
					path = new ShapePath{};
					if (!relative) {
						currentPos = { 0, 0 };
					}
					currentPos.x += args[0];
					currentPos.y += args[1];
					for (size_t i = 2; i < args.size(); i += 2) {
						vec2f endPos{ args[i], args[i+1] };
						if (relative) {
							endPos += currentPos;
						}
						if (length_sq(currentPos - endPos) > 1.0F) {
							path->segments.push_back(new LinearShapeSegment{ currentPos, endPos });
							currentPos = endPos;
						} else {
							throw std::runtime_error("degenerate");
						}
					}
					break;
				}
				case 'H':
				{
					assert(path);
					for (size_t i = 0; i < args.size(); i++) {
						vec2f endPos{ args[i], currentPos.y };
						if (relative) {
							endPos.x += currentPos.x;
						}
						if (length_sq(currentPos - endPos) > 1.0F) {
							path->segments.push_back(new LinearShapeSegment{ currentPos, endPos });
							currentPos = endPos;
						} else {
							throw std::runtime_error("degenerate");
						}
					}
					break;
				}
				case 'V':
				{
					assert(path);
					for (size_t i = 0; i < args.size(); i++) {
						vec2f endPos{ currentPos.x, args[i] };
						if (relative) {
							endPos.y += currentPos.y;
						}
						if (length_sq(currentPos - endPos) > 1.0F) {
							path->segments.push_back(new LinearShapeSegment{ currentPos, endPos });
							currentPos = endPos;
						} else {
							throw std::runtime_error("degenerate");
						}
					}
					break;
				}
				case 'L': 
				{
					assert(path);
					for (size_t i = 0; i < args.size(); i += 2) {
						vec2f endPos{ args[i], args[i+1] };
						if (relative) {
							endPos += currentPos;
						}
						if (length_sq(currentPos - endPos) > 1.0F) {
							path->segments.push_back(new LinearShapeSegment{ currentPos, endPos });
							currentPos = endPos;
						} else {
							throw std::runtime_error("degenerate");
						}
					}
				}
					break;
				case 'Z':
				{
					assert(path);
					assert(path->segments.size() >= 2);
					vec2f endPos = path->segments[0]->eval(0);
					if (length_sq(currentPos - endPos) > 1.0F) {
						path->segments.push_back(new LinearShapeSegment{ currentPos, endPos });
						currentPos = endPos;
					}
					break;
				}
				case 'Q':
				{
					assert(path);
					for (size_t i = 0; i < args.size(); i += 4) {
						vec2f handle{ args[i], args[i + 1] };
						vec2f endPos{ args[i + 2], args[i + 3] };
						if (relative) {
							handle += currentPos;
							endPos += currentPos;
						}
						path->segments.push_back(new QuadraticShapeSegment{ currentPos, handle, endPos });
						currentPos = endPos;
					}
					break;
				}
				case 'C':
				{
					assert(path);
					for (size_t i = 0; i < args.size(); i += 6) {
						vec2f handle1{ args[i], args[i+1] };
						vec2f handle2{ args[i+2], args[i+3] };
						vec2f endPos{ args[i+4], args[i+5] };
						if (relative) {
							handle1 += currentPos;
							handle2 += currentPos;
							endPos += currentPos;
						}
						path->segments.push_back(new CubicShapeSegment{ currentPos, handle1, handle2, endPos });
						currentPos = endPos;
					}
				}
					break;
				case 'A':
				{
					assert(path);
					for (size_t i = 0; i < args.size(); i += 7) {
						float xRadius = args[i];
						float yRadius = args[i + 1];
						float xAxisRotation = args[i + 2];
						bool largeArcFlag = args[i + 3] != 0;
						bool sweepFlag = args[i + 4] != 0;
						vec2f nextPos = { args[i + 5] , args[i + 6] };
						if (relative) {
							nextPos += currentPos;
						}
						create_arc(currentPos, xRadius, yRadius, xAxisRotation, largeArcFlag, sweepFlag, nextPos, path->segments);
						currentPos = nextPos;
					}
					break;
				}
				default:
					throw std::runtime_error("Command not supported!");
				}
			}

			assert(path);
			shape->paths.push_back(path);
			delete[] insns;
			return shape;
		}

		void delete_shape(Shape* shape) {
			for (ShapePath* path : shape->paths) {
				for (ShapeSegment* seg : path->segments) {
					delete seg;
				}
				delete path;
			}
			delete shape;
		}

		constexpr float SMOOTH_CUTOFF = 0.001F;

		void preprocess_shape(Shape* shape) {
			for (ShapePath* p : shape->paths) {
				RGBA8 color = p->segments.size() == 1 ? RGBA8{ 255, 255, 255 } : RGBA8{ 255, 0, 255 };
				if (p->segments.size() == 1 && fabs(p->segments[0]->direction(0).cross(p->segments[0]->direction(1))) < SMOOTH_CUTOFF) {
					p->segments[0]->color[0] = RGBA8{ 255, 255, 0 };
					p->segments[0]->color[1] = RGBA8{ 0, 255, 255 };
					p->segments[0]->colorCount = 1;
				} else {
					ShapeSegment* prev = nullptr;
					for (ShapeSegment* s : p->segments) {
						if (prev) {
							if (color == RGBA8{ 255, 255, 0 }) {
								color = RGBA8{ 0, 255, 255 };
							} else {
								color = RGBA8{ 255, 255, 0 };
							}
						}
						std::cout << static_cast<uint32_t>(color.r) << " " << static_cast<uint32_t>(color.g) << " " << static_cast<uint32_t>(color.b);
						s->print();
						std::cout << "\n";
						s->color[0] = color;
						prev = s;
					}
				}
				std::cout << "\n";
			}
		}

		void generate_image(Shape* shape, uint32_t width, uint32_t height, RGBA8* pixels) {
			vec2f pixelSize{ 1.0F/width, 1.0F/height };
			vec2f halfPixel = pixelSize * 0.5;
			for (uint32_t x = 0; x < width; x++) {
				for (uint32_t y = 0; y < height; y++) {
					vec2f pos = vec2f{ static_cast<float>(x), static_cast<float>(y) } * pixelSize * 1000.0F + halfPixel;

					SignedDistance sRed{ nullptr, FLT_MAX, -FLT_MAX };
					SignedDistance sGreen{ nullptr, FLT_MAX, -FLT_MAX };
					SignedDistance sBlue{ nullptr, FLT_MAX, -FLT_MAX };
					
					int type = 0;
					for (ShapePath* path : shape->paths) {
						for (ShapeSegment* seg : path->segments) {
							SignedDistance dist = seg->distance(pos);
							RGBA8 color = dist.segment->get_color(dist.t);

							if (color.r > 0 && dist.abs() < sRed.abs()) {
								sRed = dist;
							}
							if (color.g > 0 && dist.abs() < sGreen.abs()) {
								sGreen = dist;
							}
							if (color.b > 0 && dist.abs() < sBlue.abs()) {
								sBlue = dist;
							}
							type++;
						}
					}
					
					RGBA8 final{ 0, 0, 0, 255 };
					if (sRed.segment) {
						final.r = static_cast<uint8_t>((1 - clamp01(sRed.segment->pseudo_distance(pos, sRed.t) * 0.02F + 0.5F)) * 255);
					}
					if (sGreen.segment) {
						final.g = static_cast<uint8_t>((1 - clamp01(sGreen.segment->pseudo_distance(pos, sGreen.t) * 0.02F + 0.5F)) * 255);
					}
					if (sBlue.segment) {
						final.b = static_cast<uint8_t>((1 - clamp01(sBlue.segment->pseudo_distance(pos, sBlue.t) * 0.02F + 0.5F)) * 255);
					}
					pixels[y * width + x] = final;
					//SignedDistance dist = shape->distance(pos + halfPixel);
					//float value = 1-clamp01(dist.distance * 0.02F);
					//pixels[y * width + x] = RGBA8(255, 255, 255, static_cast<uint8_t>(value * 255));
				}
			}
		}

		float median(RGBA8 pixel) {
			return static_cast<float>(std::max(std::min(pixel.r, pixel.g), std::min(std::max(pixel.r, pixel.g), pixel.b)))/255.0F;
		}

		bool collumn_has_value(RGBA8* pixels, uint32_t width, uint32_t height, uint32_t x) {
			for (uint32_t y = 0; y < height; y++) {
				if (median(pixels[y * width + x]) >= 0.5F) {
					return true;
				}
			}
			return false;
		}

		vec2f calc_width(RGBA8* pixels, uint32_t width, uint32_t height) {
			vec2f pixelSize{ 1.0F / width, 1.0F / height };
			vec2f halfPixel = pixelSize * 0.5;
			vec2f charSize{ 0, 1 };
			for (uint32_t x = 0; x < width; x++) {
				if (collumn_has_value(pixels, width, height, x)) {
					charSize.x = static_cast<float>(x) / static_cast<float>(width);
					break;
				}
			}
			for (int32_t x = width - 1; x >= 0; x--) {
				if (collumn_has_value(pixels, width, height, x)) {
					charSize.y = static_cast<float>(x+1) / static_cast<float>(width);
					break;
				}
			}
			return charSize;
		}

		struct GlyphData {
			RGBA8* pixelArray;
			vec2f charSize;
		};

		float find_touch_offset(GlyphData& left, GlyphData& right, uint32_t width, uint32_t height) {
			uint32_t leftEdge = 0;
			uint32_t rightEdge = width;
			for (uint32_t y = 0; y < height; y ++) {
				for (int32_t x = width - 1; x >= 0; x--) {
					if (x < leftEdge) {
						break;
					}
					if (median(left.pixelArray[y * width + x]) >= 0.5F) {
						leftEdge = x + 1;
						break;
					}
				}
				for (uint32_t x = 0; x < width; x++) {
					if (x > rightEdge) {
						break;
					}
					if (median(right.pixelArray[y * width + x]) >= 0.5F) {
						rightEdge = x;
						break;
					}
				}
			}
			uint32_t difference = rightEdge + (width - leftEdge);
			return static_cast<float>(difference) / static_cast<float>(width);
		}

		//Quick and dirty program to generate a multichannel signed distance field
		//It's slow, not super stable, and doesn't have a lot of features, but it does what I want.
		//Implemented from https://github.com/Chlumsky/msdfgen/files/3050967/thesis.pdf
		void generate_MSDF(std::wstring srcFile, std::wstring dstFile, uint32_t sizeX, uint32_t sizeY, bool font) {
			FileMapping src = map_file(srcFile);
			char* text = reinterpret_cast<char*>(src.mapping);
			std::ofstream outFile;
			outFile.open(dstFile, std::ios::binary);
			RGBA8* pixels = nullptr;
			if (font) {
				std::unordered_map<char, GlyphData> glyphInfo{};
				std::unordered_map<text::CharPair, float, text::CharPairHash> kerning{};
				uint32_t iter = 0;
				while (true) {
					iter++;
					text = find_in_str(text, "GlyphLayer-");
					if (text == nullptr) {
						break;
					}
					text += strlen("GlyphLayer-");
					char glyph = *text;
					//Why these regular ascii characters are encoded in some other format is beyond me. Guess I'll just translate them manually.
					if (strncmp(text, "&lt;", 4) == 0) {
						glyph = '<';
					} else if (strncmp(text, "&gt;", 4) == 0) {
						glyph = '>';
					} else if (strncmp(text, "&quot;", 4) == 0) {
						glyph = '"';
					} else if (strncmp(text, "&amp;", 4) == 0) {
						glyph = '&';
					}
					text += 1;
					//if (glyph != ',') {
					//	continue;
					//}
					Shape* shape = parse_shape(text);
					preprocess_shape(shape);
					pixels = new RGBA8[sizeX * sizeY];
					generate_image(shape, sizeX, sizeY, pixels);

					glyphInfo[glyph] = GlyphData{ pixels, calc_width(pixels, sizeX, sizeY) };

					std::cout << "Processed Glyph: " << glyph << std::endl;
					delete_shape(shape);
					//if (iter == 3) {
					//	break;
					//}
				}
				for (std::unordered_map<char, GlyphData>::iterator it0 = glyphInfo.begin(); it0 != glyphInfo.end(); it0 ++) {
					for (std::unordered_map<char, GlyphData>::iterator it1 = glyphInfo.begin(); it1 != glyphInfo.end(); it1++) {
						text::CharPair pair{ it0->first, it1->first };
						if (kerning.count(pair)) {
							continue;
						}
						kerning[pair] = find_touch_offset(it0->second, it1->second, sizeX, sizeY);
					}
				}

				outFile.write(reinterpret_cast<const char*>(&sizeX), sizeof(uint32_t));
				outFile.write(reinterpret_cast<const char*>(&sizeY), sizeof(uint32_t));
				uint32_t numGlyphs = glyphInfo.size();
				outFile.write(reinterpret_cast<const char*>(&numGlyphs), sizeof(uint32_t));
				for (std::unordered_map<char, GlyphData>::iterator it = glyphInfo.begin(); it != glyphInfo.end(); it++) {
					outFile.write(&it->first, sizeof(char));
					outFile.write("\0\0\0", 3);
					float minX = it->second.charSize.x;
					float maxX = it->second.charSize.y;
					outFile.write(reinterpret_cast<const char*>(&minX), sizeof(float));
					outFile.write(reinterpret_cast<const char*>(&maxX), sizeof(float));
				}
				for (std::unordered_map<text::CharPair, float, text::CharPairHash>::iterator it = kerning.begin(); it != kerning.end(); it++) {
					outFile.write(&it->first.left, sizeof(char));
					outFile.write(&it->first.right, sizeof(char));
					outFile.write("\0\0", 2);
					outFile.write(reinterpret_cast<const char*>(&it->second), sizeof(float));
				}
				for (std::unordered_map<char, GlyphData>::iterator it = glyphInfo.begin(); it != glyphInfo.end(); it++) {
					stbi_write_png((std::string("./resources/textures/testfont-") + it->first + ((it->first > 'A' && it->first < 'Z') ? "_cap" : "") + ".png").c_str(), sizeX, sizeY, STBI_rgb_alpha, it->second.pixelArray, 0);
					outFile.write(reinterpret_cast<const char*>(it->second.pixelArray), sizeof(RGBA8) * sizeX * sizeY);
					std::cout << "Wrote Glyph: " << it->first << std::endl;
					delete it->second.pixelArray;
				}
				pixels = nullptr;
			} else {
				outFile.write(reinterpret_cast<const char*>(&sizeX), sizeof(uint32_t));
				outFile.write(reinterpret_cast<const char*>(&sizeY), sizeof(uint32_t));
				pixels = new RGBA8[sizeX * sizeY];
				char* layer1 = find_in_str(text, "layer1");
				Shape* shape = parse_shape(layer1);
				preprocess_shape(shape);
				generate_image(shape, sizeX, sizeY, pixels);
				stbi_write_png("./resources/textures/testdistance.png", sizeX, sizeY, STBI_rgb_alpha, pixels, 0);
				outFile.write(reinterpret_cast<const char*>(pixels), sizeof(RGBA8) * sizeX * sizeY);
				delete_shape(shape);
			}
			outFile.close();
			delete[] pixels;
			unmap_file(src);
		}
	}
}
