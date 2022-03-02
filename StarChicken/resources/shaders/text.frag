#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout (set = 0, binding = 0) uniform sampler bilinearSampler;
layout (set = 0, binding = 1) uniform texture2D textures[];

layout (location = 0) in vec2 texCoord;
layout (location = 1) in float cutoff;
layout (location = 2) in vec4 color;
layout (location = 3) flat in uint texIdx;

layout (location = 0) out vec4 FragColor;

float median(float a, float b, float c){
	return max(min(a, b), min(max(a, b), c));
}

float screen_px_range(){
	vec2 unitRange = vec2(2.5)/vec2(textureSize(sampler2D(textures[texIdx], bilinearSampler), 0));
	vec2 screenTexSize = vec2(1.0)/fwidth(texCoord);
	return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

void main() {
	vec4 msdf = texture(sampler2D(textures[texIdx], bilinearSampler), vec2(texCoord.x, texCoord.y));
	float val = median(msdf.r, msdf.g, msdf.b);
	/*if(val > cutoff){
		FragColor = color;
	} else {
		discard;
	}*/
	
	FragColor = vec4(clamp((val - cutoff)*screen_px_range() + cutoff, 0, 1)) * color;
	if(val == 0){
		discard;
	}
	//FragColor = mix(vec4(clamp((val - cutoff)*10, 0, 1)), vec4(1.0), 0.999);
	//FragColor = vec4(texCoord, 0, 1);
}