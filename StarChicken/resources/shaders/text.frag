#version 460
#extension GL_EXT_nonuniform_qualifier : require

const vec2 multisampleCoords[4] = vec2[4](vec2(-0.3, -0.1), vec2(-0.1, 0.3), vec2(0.3, 0.1), vec2(0.1, -0.3));
const vec2 subpixelSampleCoords[3] = vec2[3](vec2(0.3333, 0), vec2(0, 0), vec2(-0.3333, 0));
const vec2 subpixelMultisampleCoords[9] = vec2[9](vec2(-0.4, -0.4), vec2(-0.3, -0.1), vec2(-0.2, 0.2), vec2(-0.1, -0.3), vec2(0, 0), vec2(0.1, 0.3), vec2(0.2, -0.2), vec2(0.3, 0.1), vec2(0.4, 0.4));
const uint STRING_FLAG_MULTISAMPLE = 1;
const uint STRING_FLAG_SUBPIXEL = 2;

layout (set = 0, binding = 0) uniform sampler bilinearSampler;
layout (set = 0, binding = 1) uniform texture2D textures[];

layout (location = 0) in vec2 texCoord;
layout (location = 1) in float cutoff;
layout (location = 2) in vec4 color;
layout (location = 3) flat in uint texIdx;
layout (location = 4) flat in uint flags;

layout (location = 0) out vec4 FragColor;

float median(float a, float b, float c){
	return max(min(a, b), min(max(a, b), c));
}

float screen_px_range(vec2 fragWidth){
	vec2 unitRange = vec2(2.5)/vec2(textureSize(sampler2D(textures[texIdx], bilinearSampler), 0));
	vec2 screenTexSize = vec2(1.0)/fragWidth;
	return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

vec4 sample_color(float screenPxRange, vec2 coord){
	vec4 msdf = texture(sampler2D(textures[texIdx], bilinearSampler), coord);
	float val = median(msdf.r, msdf.g, msdf.b);
	return vec4(1, 1, 1, clamp((val - cutoff) * screenPxRange + cutoff, 0, 1));
}

void main() {
	
	/*if(val > cutoff){
		FragColor = color;
	} else {
		discard;
	}*/
	vec2 fragWidth = fwidth(texCoord);
	
	//FragColor = vec4(clamp((val - cutoff)*screen_px_range(fwidth(texCoord), 0.5) + cutoff, 0, 1)) * color;
	vec4 val;
	if((flags & STRING_FLAG_SUBPIXEL) > 0 && (flags & STRING_FLAG_MULTISAMPLE) > 0){
		float screenPxRange = screen_px_range(fragWidth);
		vec2 sampleR = (
		sample_color(screenPxRange, texCoord + subpixelMultisampleCoords[0] * fragWidth).ra + 
		sample_color(screenPxRange, texCoord + subpixelMultisampleCoords[1] * fragWidth).ra +
		sample_color(screenPxRange, texCoord + subpixelMultisampleCoords[2] * fragWidth).ra
		) * vec2(color.r, 1);
		vec2 sampleG = (
		sample_color(screenPxRange, texCoord + subpixelMultisampleCoords[3] * fragWidth).ga + 
		sample_color(screenPxRange, texCoord + subpixelMultisampleCoords[4] * fragWidth).ga +
		sample_color(screenPxRange, texCoord + subpixelMultisampleCoords[5] * fragWidth).ga
		) * vec2(color.g, 1);
		vec2 sampleB = (
		sample_color(screenPxRange, texCoord + subpixelMultisampleCoords[6] * fragWidth).ba + 
		sample_color(screenPxRange, texCoord + subpixelMultisampleCoords[7] * fragWidth).ba +
		sample_color(screenPxRange, texCoord + subpixelMultisampleCoords[8] * fragWidth).ba
		) * vec2(color.b, 1);
		// * 0.25 * color;
		val = vec4(vec3(sampleR[0] * sampleR[1], sampleG[0] * sampleG[1], sampleB[0] * sampleB[1]) * 0.33333, 1);
	} if((flags & STRING_FLAG_SUBPIXEL) > 0) {
		float screenPxRange = screen_px_range(fragWidth);
		vec2 sampleR = sample_color(screenPxRange, texCoord + subpixelSampleCoords[0] * fragWidth).ra * vec2(color.r, 1);
		vec2 sampleG = sample_color(screenPxRange, texCoord + subpixelSampleCoords[1] * fragWidth).ga * vec2(color.g, 1);
		vec2 sampleB = sample_color(screenPxRange, texCoord + subpixelSampleCoords[2] * fragWidth).ba * vec2(color.b, 1);
		// * 0.25 * color;
		val = vec4(sampleR[0] * sampleR[1], sampleG[0] * sampleG[1], sampleB[0] * sampleB[1], 1);
	} else if((flags & STRING_FLAG_MULTISAMPLE) > 0){
		float screenPxRange = screen_px_range(fragWidth) * 2;
		val = (
		sample_color(screenPxRange, texCoord + multisampleCoords[0] * fragWidth) + 
		sample_color(screenPxRange, texCoord + multisampleCoords[1] * fragWidth) + 
		sample_color(screenPxRange, texCoord + multisampleCoords[2] * fragWidth) + 
		sample_color(screenPxRange, texCoord + multisampleCoords[3] * fragWidth)
		) * 0.25 * color;
	} else {
		float screenPxRange = screen_px_range(fragWidth);
		val = sample_color(screenPxRange, texCoord) * color;
	}
	
	if(val.a == 0){
		discard;
	}
	FragColor = val;
	//FragColor = mix(vec4(clamp((val - cutoff)*10, 0, 1)), vec4(1.0), 0.999);
	//FragColor = vec4(texCoord, 0, 1);
}