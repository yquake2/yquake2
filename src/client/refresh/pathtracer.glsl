
#ifndef NUM_BOUNCES
# define NUM_BOUNCES 0
#endif

#ifndef NUM_SHADOW_SAMPLES
# define NUM_SHADOW_SAMPLES 1
#endif

#ifndef NUM_LIGHT_SAMPLES
# define NUM_LIGHT_SAMPLES 1
#endif
	
#ifndef NUM_SKY_SAMPLES
# define NUM_SKY_SAMPLES 1
#endif

#ifndef NUM_AO_SAMPLES
# define NUM_AO_SAMPLES 0
#endif

#ifndef TRI_SHADOWS_ENABLE
# define TRI_SHADOWS_ENABLE 1
#endif

#ifndef DIFFUSE_MAP_ENABLE
# define DIFFUSE_MAP_ENABLE 1
#endif

#ifndef RAND_TEX_SIZE
# define RAND_TEX_SIZE 64
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
uniform sampler2DArray randtex;
uniform int frame = 0;
uniform float ao_radius = 150.0;
uniform vec3 ao_color = vec3(1);
uniform float bounce_factor = 0.75;

in vec4 texcoords[5], color;

vec4 out_pln;
float rand_index = frame;

vec2 rand()
{
	return texture(randtex, vec3(gl_FragCoord.st / RAND_TEX_SIZE, mod(rand_index++, 8))).rg;
}

vec2 boxInterval(vec3 ro, vec3 rd, vec3 size)
{
	vec3 mins = (size * -sign(rd) - ro) / rd;
	vec3 maxs = (size * +sign(rd) - ro) / rd;	
	return vec2(max(max(mins.x, mins.y), mins.z), min(min(maxs.x, maxs.y), maxs.z));
}

bool traceRayShadowTri(vec3 ro, vec3 rd, float maxdist)
{
#if TRI_SHADOWS_ENABLE
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
				vec3 p2 = texelFetch(edge0, tri.y & 0xffff).xyz;

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
#endif

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

bool traceRayShadowBSP(vec3 org, vec3 dir, float t0, float max_t)
{
	return traceRayBSP(org, dir, t0, max_t) >= max_t;
}

ivec2 getLightRef(vec3 p)
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
			 return ivec2(0);
		
	} while (node != vec2(0));
	
	ivec4 light_indices = texture(bsp_lightrefs, prev_node);
	
	return d < 0.0 ? light_indices.yw : light_indices.xz;
}

vec3 sampleDirectLight(vec3 rp, vec3 rn, int oli)
{
	vec3 r = vec3(0);
	oli = oli < 0 ? -oli : +oli;
	int li = oli;
	int ref = texelFetch(lightrefs, li).r;
	
	if (ref != -1)
	{
		float wsum = 0.0;
		
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
		
		for (int light_sample = 0; light_sample < NUM_LIGHT_SAMPLES; ++light_sample)
		{
			li = oli;
			ref = texelFetch(lightrefs, li).r;
			
			vec3 p0, p1, p2, n;
		
			float x = rand().x * wsum, w=1.0;
			vec4 j = vec4(0);

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

#if NUM_SHADOW_SAMPLES > 0
			for (int shadow_sample = 0; shadow_sample < NUM_SHADOW_SAMPLES; ++shadow_sample)
			{
				vec4 light = j;
				vec3 sp = rp;
				vec3 sn = rn.xyz;
				vec3 lp = p0;
				vec2 uv = rand();
				
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
#else
			r += abs(light.rgb) * wsum / w;
#endif
		}
	}

	return r / float(NUM_LIGHT_SAMPLES * NUM_SHADOW_SAMPLES);
}


void main()
{
	vec4 pln;

	gl_FragColor = vec4(0);

	vec3 rp = texcoords[1].xyz + texcoords[3].xyz * EPS * 16;

	pln.xyz = texcoords[3].xyz;
	pln.w = dot(rp, pln.xyz);

	int rpli = getLightRef(rp).x;
	vec3 r = sampleDirectLight(rp, pln.xyz, rpli);
	
	r += texcoords[4].rgb;

#if NUM_BOUNCES > 0 || NUM_AO_SAMPLES > 0 || NUM_SKY_SAMPLES > 0
	vec3 u = normalize(cross(pln.xyz, pln.zxy));
	vec3 v = cross(pln.xyz, u);
#endif
	
#if NUM_AO_SAMPLES > 0
	{
		float ao = 0.0;
		for (int ao_sample = 0; ao_sample < NUM_AO_SAMPLES; ++ao_sample)
		{
			vec2 rr = rand();
			
			float r1 = 2.0 * PI * rr.x;
			float r2s = sqrt(rr.y);

			vec3 rd = u * cos(r1) * r2s + v * sin(r1) * r2s + pln.xyz * sqrt(1.0 - rr.y);
				
			if (traceRayShadowBSP(rp, rd, EPS * 16, ao_radius) && traceRayShadowTri(rp, rd, ao_radius))
				ao += 1.0;
		}
		gl_FragColor.rgb += ao_color * ao / float(NUM_AO_SAMPLES);
	}
#endif

#if NUM_BOUNCES > 0 || NUM_SKY_SAMPLES > 0

	vec3 sp, rd;
	float t;

#if NUM_BOUNCES == 0
	if (rpli < 0)
#endif
	{
		vec2 rr = rand();

		float r1 = 2.0 * PI * rr.x;
		float r2s = sqrt(rr.y);
		
		rd = normalize(u * cos(r1) * r2s + v * sin(r1) * r2s + pln.xyz * sqrt(1.0 - rr.y));

		t = traceRayBSP(rp, rd, EPS * 16, 2048.0) - 1.0;

		if ((dot(out_pln.xyz, rp) - out_pln.w) < 0.0)
			out_pln *= -1.0;
		
		sp = rp + rd * max(0.0, t);
	}
	
#if NUM_BOUNCES > 0
	if (traceRayShadowTri(rp, rd, t))
	{
		int li = getLightRef(sp).x;
		pln = out_pln;
		r += bounce_factor * sampleDirectLight(sp, out_pln.xyz, li);
		out_pln = pln;
	}
#endif

#if NUM_SKY_SAMPLES > 0
	if (rpli < 0)
	{
		vec3 sky_r = vec3(0);
		for (int sky_sample = 0; sky_sample < NUM_SKY_SAMPLES; ++sky_sample)
		{
			int li = getLightRef(sp).y;
			int ref = texelFetch(lightrefs, li).r;
			if (ref != -1)
			{
				if (traceRayShadowTri(rp, rd, t))
				{
					do
					{
						vec4 light = texelFetch(lights, ref);
						ivec2 tri = texelFetch(triangle, floatBitsToInt(light.w)).xy;
						
						vec3 p0 = texelFetch(edge0, tri.x & 0xffff).xyz;
						vec3 p1 = texelFetch(edge0, tri.x >> 16).xyz;
						vec3 p2 = texelFetch(edge0, tri.y).xyz;
						
						vec3 n = normalize(cross(p1 - p0, p2 - p0));
						
						vec3 sp2 = sp;
						
						float s1 = dot(cross(p1 - sp2, p2 - sp2), n);

						if (dot(light.rgb, vec3(1)) < 0.0 && s1 < 0.0)
						{
							s1 *= -1.0;
							vec3 mirror = normalize(p2 - p1);
							sp2 -= 2.0 * mirror * dot(sp2 - (p1 + p2) * 0.5, mirror);
						}

						float s0 = dot(cross(p0 - sp2, p1 - sp2), n);
						float s2 = dot(cross(p2 - sp2, p0 - sp2), n);

						if (s0 > 0.0 && s1 > 0.0 && s2 > 0.0 && abs(dot(n, sp2 - p0)) < 1.0)
							sky_r += abs(light.rgb);
						
						++li;
						ref = texelFetch(lightrefs, li).r;
						
					} while (ref != -1);
				}
			}

#if NUM_SKY_SAMPLES > 1			
			vec2 rr = rand();

			float r1 = 2.0 * PI * rr.x;
			float r2s = sqrt(rr.y);

			rd = normalize(u * cos(r1) * r2s + v * sin(r1) * r2s + pln.xyz * sqrt(1.0 - rr.y));

			t = traceRayBSP(rp, rd, EPS * 16, 2048.0) - 1.0;

			if ((dot(out_pln.xyz, rp) - out_pln.w) < 0.0)
				out_pln *= -1.0;
			
			sp = rp + rd * max(0.0, t);
#endif
		}
		r += sky_r / float(NUM_SKY_SAMPLES);
	}
#endif

#if NUM_BOUNCES > 1
	{
		float factor = bounce_factor;
		rp = sp;
		for (int bounce = 1; bounce < NUM_BOUNCES; ++bounce)
		{
			vec2 rr = rand();

			float r1 = 2.0 * PI * rr.x;
			float r2s = sqrt(rr.y);

			vec3 rd = normalize(u * cos(r1) * r2s + v * sin(r1) * r2s + out_pln.xyz * sqrt(1.0 - rr.y));

			t = traceRayBSP(rp, rd, EPS * 16, 2048.0) - 1.0;

			if (!traceRayShadowTri(rp, rd, t))
				break;
			
			if ((dot(out_pln.xyz, rp) - out_pln.w) < 0.0)
				out_pln *= -1.0;
			
			rp = rp + rd * max(0.0, t);
			
			factor *= bounce_factor;
			int li = getLightRef(rp).x;
			pln = out_pln;
			r += factor * sampleDirectLight(rp, out_pln.xyz, li);
			out_pln = pln;
			
#if NUM_BOUNCES > 2
			u = normalize(cross(out_pln.xyz, out_pln.zxy));
			v = cross(out_pln.xyz, u);
#endif
		}
	}
#endif

#endif

	gl_FragColor.rgb += r / 1024.;

	gl_FragColor.a = color.a;
	
#if DIFFUSE_MAP_ENABLE
	gl_FragColor.rgb *= texture(tex0, texcoords[0].st).rgb + vec3(1e-2);
#endif

	gl_FragColor.rgb = sqrt(gl_FragColor.rgb);
}
