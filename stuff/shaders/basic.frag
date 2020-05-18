#version 450

layout(push_constant) uniform PushConstant
{
	// vertex shader has 'mat4 vpMatrix;' at begin.
	layout(offset = 68) float gamma;
} pc;

layout(set = 0, binding = 0) uniform sampler2D sTexture;

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec4 color;
layout(location = 2) in float aTreshold;

layout(location = 0) out vec4 fragmentColor;

void main()
{
	fragmentColor = texture(sTexture, texCoord) * color;
	if(fragmentColor.a < aTreshold)
		discard;

	fragmentColor = vec4(pow(fragmentColor.rgb, vec3(pc.gamma)), fragmentColor.a);
}
