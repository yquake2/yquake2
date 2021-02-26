#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstant
{
	layout(offset = 68) float postprocess;
	layout(offset = 72) float gamma;
	layout(offset = 76) float scrWidth;
	layout(offset = 80) float scrHeight;
	layout(offset = 84) float offsetX;
	layout(offset = 88) float offsetY;
} pc;

layout(set = 0, binding = 0) uniform sampler2D sTexture;

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragmentColor;

void main()
{
	vec2 unnormTexCoord = texCoord * vec2(pc.scrWidth, pc.scrHeight) + vec2(pc.offsetX, pc.offsetY);

	// apply any additional world-only postprocessing effects here (if enabled)
	if (pc.postprocess > 0.0)
	{
		//gamma + color intensity bump
		fragmentColor = vec4(pow(textureLod(sTexture, unnormTexCoord, 0.0).rgb * 1.5, vec3(pc.gamma)), 1.0);
	}
	else
	{
		fragmentColor = textureLod(sTexture, unnormTexCoord, 0.0);
	}
}
