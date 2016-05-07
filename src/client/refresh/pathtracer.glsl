#version 330
	
#ifndef NUM_BOUNCES
# define NUM_BOUNCES 0
#endif

#ifndef NUM_SHADOW_SAMPLES
# define NUM_SHADOW_SAMPLES 1
#endif

#ifndef NUM_LIGHT_SAMPLES
# define NUM_LIGHT_SAMPLES 1
#endif
	
#ifndef SKY_ENABLE
# define SKY_ENABLE 1
#endif

#ifndef AO_SAMPLES
# define AO_SAMPLES 0
#endif

#ifndef TRI_SHADOWS_ENABLE
# define TRI_SHADOWS_ENABLE 1
#endif
	
#define EPS		(1.0 / 32.0)
#define MAXT	2048.0
#define PI		acos(-1.0)

uniform sampler2D tex0;
uniform sampler2D planes;
uniform sampler2D branches;
uniform isamplerBuffer node0;
uniform isamplerBuffer node1;
uniform samplerBuffer edge0;
uniform isamplerBuffer triangle;
uniform samplerBuffer lights;
uniform isamplerBuffer lightrefs;
uniform isampler2D bsp_lightrefs;
uniform int frame = 0;
uniform float ao_radius = 150.0;
uniform vec3 ao_color = vec3(1);
uniform float bounce_factor = 0.75;

in vec4 texcoords[5], color;

vec3 rp = texcoords[1].xyz;
vec3 dir = normalize(texcoords[2].xyz);
vec3 normal = texcoords[3].xyz;
vec4 out_pln;
int sky_li;
float rand_seed = 0.0;

float rand()
{
	return fract(sin(rand_seed++) * 43758.5453123);
}

vec2 boxInterval(vec3 ro, vec3 rd, vec3 size)
{
	vec3 mins = (size * -sign(rd) - ro) / rd;
	vec3 maxs = (size * +sign(rd) - ro) / rd;	
	return vec2(max(max(mins.x, mins.y), mins.z), min(min(maxs.x, maxs.y), maxs.z));
}

bool traceRayShadowTri(vec3 ro, vec3 rd, float maxdist)
{
	int node = 0;

	do
	{
		ivec4 n0 = texelFetch(node0, node);
		ivec4 n1 = texelFetch(node1, node);

		vec2 i = boxInterval(ro - intBitsToFloat(n1.xyz), rd, intBitsToFloat(n0.xyz));

		if (i.x < i.y && i.x < maxdist && i.y > 0.0)
		{
			if (n1.w != -1)
			{
				ivec2 tri = texelFetch(triangle, n1.w).xy;

				vec3 p0 = texelFetch(edge0, tri.x & 0xffff).xyz;
				vec3 p1 = texelFetch(edge0, tri.x >> 16).xyz;
				vec3 p2 = texelFetch(edge0, tri.y).xyz;

				vec3 n = cross(p1 - p0, p2 - p0);
				float t = dot(p0 - ro, n) / dot(rd, n);

				float d0 = dot(rd, cross(p0 - ro, p1 - ro));
				float d1 = dot(rd, cross(p1 - ro, p2 - ro));
				float d2 = dot(rd, cross(p2 - ro, p0 - ro));

				if (sign(d0) == sign(d1) && sign(d0) == sign(d2) && t < maxdist && t > 0.0)
					return false;
			}
			
			++node;
		}
		else
			node = n0.w;

	} while (node > 0);

	return true;
}

bool traceRayShadowBSP(vec3 org, vec3 dir, float t0, float max_t)
{
	vec2  other_node = vec2(0);
	float other_t1 = max_t;

	while (t0 < max_t)
	{
		vec2  node = other_node;
		float t1 = other_t1;
		
		other_node = vec2(0);
		other_t1 = max_t;

		do
		{
			vec4 pln = texture(planes, node);
			vec4 children = texture(branches, node);
			
			float t = dot(pln.xyz, dir);

			children = (t > 0.0) ? children.zwxy : children.xyzw;

			t = (pln.w - dot(pln.xyz, org)) / t;

			if (t > t0)
			{
				node = children.xy;
				
				if (t < t1) 
				{
					other_t1 = t1;
					t1 = t;
					other_node = children.zw;
				}
			}
			else
			{
				node = children.zw;
			}
			
			if (node.y == 1.0)
			{
				 return false;
			}
			
		} while (node != vec2(0.0));
		
		t0 = t1 + EPS;
	}

	return true;
}

float traceRayBSP(vec3 org, vec3 dir, float t0, float max_t)
{
	vec2  other_node = vec2(0);
	float other_t1 = max_t;
	vec4 other_pln0;

	while (t0 < max_t)
	{
		vec2  node = other_node;
		float t1 = other_t1;
		vec4 pln0 = other_pln0;
		
		other_node = vec2(0);
		other_t1 = max_t;

		do
		{
			vec4 pln = texture(planes, node);
			vec4 children = texture(branches, node);
			
			float t = dot(pln.xyz, dir);

			children = (t > 0.0) ? children.zwxy : children.xyzw;

			t = (pln.w - dot(pln.xyz, org)) / t;

			if (t > t0)
			{
				node = children.xy;
				
				if (t < t1) 
				{
					other_t1 = t1;
					t1 = t;
					other_pln0 = pln0;
					pln0 = pln;
					other_node = children.zw;
				}
			}
			else
			{
				node = children.zw;
			}
			
			if (node.y == 1.0)
			{
				 return t0;
			}
			
		} while (node != vec2(0));
		
		out_pln = pln0;
		t0 = t1 + EPS;
	}

	return 1e8;
}

int getLightRef(vec3 p)
{
	vec2 node = vec2(0), prev_node;
	float d;
	
	do
	{
		vec4 pln = texture(planes, node);
		vec4 children = texture(branches, node);
		
		d = dot(pln.xyz, p) - pln.w;
		
		if (d < 0.0)
		{
			prev_node = node;
			node = children.zw;
		}
		else
		{
			prev_node = node;
			node = children.xy;
		}

		if (node.y == 1.0)
		{
			 return -1;
		}
		
	} while (node != vec2(0));
	
	ivec2 light_indices = texture(bsp_lightrefs, prev_node).rg;
	
	return d < 0.0 ? light_indices.y : light_indices.x;
}

vec3 sampleDirectLight(vec3 rp, vec3 rn)
{
	vec3 r = vec3(0);
	int oli = getLightRef(rp);
	int li = oli;
	int ref = texelFetch(lightrefs, li).r;
	float wsum = 0.0;
	
	if (ref != -1)
	{
		do
		{
			vec4 light = texelFetch(lights, ref);

			ivec2 tri = texelFetch(triangle, floatBitsToInt(light.w)).xy;

			vec3 p0 = texelFetch(edge0, tri.x & 0xffff).xyz;
			vec3 p1 = texelFetch(edge0, tri.x >> 16).xyz;
			vec3 p2 = texelFetch(edge0, tri.y).xyz;

			vec3 n = cross(p2 - p0, p1 - p0);

			float d = distance(rp, (p0 + p1 + p2) / 3.0);
			float pd = dot(rp - p0, n);
			float w = length(light.rgb) * 1.0 / (d * d) * sqrt(max(0.0, pd));

			wsum += w;
			++li;
			ref = texelFetch(lightrefs, li).r;
			
		} while (ref != -1);
	}
	
	sky_li = li;
	float x = rand() * wsum, w=1.0;
	vec4 j = vec4(0);

	for (int light_sample = 0; light_sample < NUM_LIGHT_SAMPLES; ++light_sample)
	{
		li = oli;
		ref = texelFetch(lightrefs, li).r;
		
		vec3 p0, p1, p2, n;
		
		if (ref != -1) 
		{
			do
			{
				vec4 light=texelFetch(lights, ref);
				ivec2 tri = texelFetch(triangle, floatBitsToInt(light.w)).xy;
				
				p0 = texelFetch(edge0, tri.x & 0xffff).xyz;
				p1 = texelFetch(edge0, tri.x >> 16).xyz;
				p2 = texelFetch(edge0, tri.y).xyz;
				
				n = cross(p2 - p0, p1 - p0);
				
				float d = distance(rp, (p0 + p1 + p2) / 3.);
				float pd = dot(rp - p0, n);
				
				w = length(light.rgb) * 1.0 / (d * d) * sqrt(max(0.0, pd));	
				
				x -= w;
				
				if (x < 0.0)
				{
					j = light;
					break;
				}
				
				++li;
				ref = texelFetch(lightrefs, li).r;
				
			} while (ref != -1);
		}

		for (int shadow_sample = 0; shadow_sample < NUM_SHADOW_SAMPLES; ++shadow_sample)
		{
			vec4 light = j;
			vec3 sp = rp;
			vec3 sn = rn.xyz;
			vec3 lp = p0;
			vec2 uv = vec2(rand(), rand());
			
			if ((uv.x + uv.y) > 1.0 && dot(light.rgb, vec3(1)) > 0.0)
				uv = vec2(1) - uv;
			
			lp += (p1 - p0) * uv.x + (p2 - p0) * uv.y;
			
			float ld = distance(sp, lp);
			
			vec3 l = (lp - sp) / ld;
			float ndotl = dot(l, sn), lndotl = dot(-l, n);
			if (ndotl > 0.0 && lndotl > 0.0)
			{
				float s = (traceRayShadowBSP(sp, l, EPS * 16, ld) && traceRayShadowTri(sp, l, ld)) ? 1.0 / (ld * ld) : 0.0;
				r += s * ndotl * lndotl * abs(light.rgb) * wsum / w;
			}
		}
	}
	
	return r / float(NUM_LIGHT_SAMPLES * NUM_SHADOW_SAMPLES);
}


void main()
{
	rand_seed = gl_FragCoord.x * 89.9 + gl_FragCoord.y * 197.3 + frame * 0.02;

	rp += normal * EPS * 16;

	out_pln.xyz = normal;
	out_pln.w = dot(rp, out_pln.xyz);

	vec4 spln = out_pln;

	vec3 b = vec3(spln.z, spln.x, spln.y);
	vec3 u = normalize(cross(spln.xyz, b));
	vec3 v = cross(spln.xyz, u);
	vec3 r = texcoords[4].rgb;
	  

	r += sampleDirectLight(rp, spln.xyz);
	

	float r1 = 2.0 * PI * rand();
	float r2 = rand();
	float r2s = sqrt(r2);

	vec3 rd = normalize(u * cos(r1) * r2s + v * sin(r1) * r2s + spln.xyz * sqrt(1.0 - r2));

	float t = traceRayBSP(rp, rd, EPS * 16, 2048.0);

	if ((dot(out_pln.xyz, rp) - out_pln.w) < 0.0)
		out_pln *= -1.0;
	
	r += bounce_factor * sampleDirectLight(rp + rd * max(0.0, t - 1.0), out_pln.xyz);


	int li = sky_li;
	++li;
	int ref = texelFetch(lightrefs, li).r;
	if (ref != -1)
	{
		if (traceRayShadowTri(rp, rd, t))
		{
			do
			{
				vec3 sp = rp + rd * max(0.0, t - 1.0);
				
				vec4 light = texelFetch(lights, ref);
				ivec2 tri = texelFetch(triangle, floatBitsToInt(light.w)).xy;
				
				vec3 p0 = texelFetch(edge0, tri.x & 0xffff).xyz;
				vec3 p1 = texelFetch(edge0, tri.x >> 16).xyz;
				vec3 p2 = texelFetch(edge0, tri.y).xyz;
				
				vec3 n = normalize(cross(p2 - p0, p1 - p0));
				
				if (dot(light.rgb, vec3(1)) < 0.0)
				{
					vec3 mirror = normalize(cross(p2 - p1, n));
					sp -= 2.0 * mirror * dot(sp - p1, mirror);
				}
				
				float s0 = dot(cross(p0 - sp, p1 - sp), n);
				float s1 = dot(cross(p1 - sp, p2 - sp), n);
				float s2 = dot(cross(p2 - sp, p0 - sp), n);

				if (s0 < 0.0 && s1 < 0.0 && s2 < 0.0 && abs(dot(n, sp - p0)) < 1.0)
					r += light.rgb;
				
				++li;
				ref = texelFetch(lightrefs, li).r;
				
			} while (ref != -1);
		}
	}


	gl_FragColor.rgb = r / 1024.;

#if AO_SAMPLES
	{
		float ao = 0.0;
		for (int ao_sample = 0; ao_sample < AO_SAMPLES; ++ao_sample)
		{
			float r1 = 2.0 * PI * rand();
			float r2 = rand();
			float r2s = sqrt(r2);

			vec3 rd = u * cos(r1) * r2s + v * sin(r1) * r2s + spln.xyz * sqrt(1.0 - r2);
				
			if (traceRayShadowBSP(rp, rd, EPS * 16, aoradius) && traceRayShadowTri(rp, rd, aoradius))
				r += 1.0;
		}
		gl_FragColor.rgb += ao_color * ao / float(AO_SAMPLES);
	}
#endif

	gl_FragColor.a = color.a;
	gl_FragColor.rgb *= texture(tex0, texcoords[0].st).rgb + vec3(1e-2);
	gl_FragColor.rgb = sqrt(gl_FragColor.rgb);
}
