/*
 * Copyright (C) 2016 Edd Biddulph
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * =======================================================================
 *
 * GPU-based pathtracing - Ray intersection and shading.
 *
 * =======================================================================
 */
 
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

#ifndef RAND_TEX_LAYERS
# define RAND_TEX_LAYERS 1
#endif

#ifndef BLUENOISE_TEX_WIDTH
# define BLUENOISE_TEX_WIDTH 64
#endif

#ifndef BLUENOISE_TEX_HEIGHT
# define BLUENOISE_TEX_HEIGHT 64
#endif

#ifndef TAA_ENABLE
# define TAA_ENABLE 0
#endif

#define EPS		(1.0 / 32.0)
#define MAXT	2048.0
#define PI		acos(-1.0)

// Diffuse (albedo) texture map.
uniform sampler2D diffuse_texture;

// BSP tree data.
uniform sampler2D		bsp_planes;
uniform sampler2D 	bsp_branches;
uniform isampler2D	bsp_lightrefs;

// Triangle mesh data.
uniform isamplerBuffer 	tri_nodes0;
uniform isamplerBuffer 	tri_nodes1;
uniform samplerBuffer 	tri_vertices;
uniform isamplerBuffer 	triangles;

#if TAA_ENABLE
// Triangle mesh data from the previous frame.
uniform isamplerBuffer 	tri_nodes0_prev;
uniform isamplerBuffer 	tri_nodes1_prev;
uniform samplerBuffer 	tri_vertices_prev;
uniform isamplerBuffer 	triangles_prev;
#endif

// Lightsource data.
uniform samplerBuffer 	lights;
uniform isamplerBuffer 	lightrefs;

// PRNG table.
uniform sampler1D randtex;
uniform sampler2DArray bluenoise;

// TAA feedback textures and transformations.
uniform sampler2D taa_world;
uniform mat4 current_world_matrix = mat4(1);
uniform mat4 previous_world_matrix = mat4(1);
uniform vec3 previous_view_origin = vec3(0);

// Uniform attributes.
uniform int		frame = 0;
uniform float	ao_radius = 150.0;
uniform vec3	ao_color = vec3(1);
uniform float	bounce_factor = 0.75;
uniform float	exposure = 1.5;
uniform float	gamma = 2.2;
uniform float	bump_factor = 0.045;

// Inputs from the vertex stage.
in vec4 texcoords[8], color;

// Globals.
vec4 out_pln;
vec3 bluenoise_sample = texture(bluenoise, vec3(gl_FragCoord.st / vec2(BLUENOISE_TEX_WIDTH, BLUENOISE_TEX_HEIGHT), mod(frame, RAND_TEX_LAYERS))).rgb;

// Produces a pair of random numbers.
vec2 rand(int index)
{
	return fract(texelFetch(randtex, index, 0).rg + bluenoise_sample.rg);
}

// Returns the interval along the given ray which intersects an axis-aligned cuboid of the given size centered at the origin.
vec2 boxInterval(vec3 ro, vec3 rd, vec3 size)
{
	vec3 mins = (size * -sign(rd) - ro) / rd;
	vec3 maxs = (size * +sign(rd) - ro) / rd;	
	return vec2(max(max(mins.x, mins.y), mins.z), min(min(maxs.x, maxs.y), maxs.z));
}

// Returns false if the given ray intersects any triangle mesh in the scene and the distance
// to the intersection along the ray is less than maxdist, otherwise returns true.
bool traceRayShadowTri(vec3 ro, vec3 rd, float maxdist,
	isamplerBuffer nodes0, isamplerBuffer nodes1, samplerBuffer vertices, isamplerBuffer tris)
{
#if TRI_SHADOWS_ENABLE
	// Start at the root node.
	int node = 0;

	do
	{
		ivec4 n0 = texelFetch(nodes0, node);
		ivec4 n1 = texelFetch(nodes1, node);

		// Test for intersection with the bounding box.
		vec2 i = boxInterval(ro - intBitsToFloat(n1.xyz), rd, intBitsToFloat(n0.xyz));

		if (i.x < i.y && i.x < maxdist && i.y > 0.0)
		{
			// The ray does intersect this node, so don't skip it.

			if (n1.w != -1)
			{
				// The node is a leaf node, so test for intersection with the referenced triangle.
				ivec2 tri = texelFetch(tris, n1.w).xy;

				vec3 p0 = texelFetch(vertices, tri.x & 0xffff).xyz;
				vec3 p1 = texelFetch(vertices, tri.x >> 16).xyz;
				vec3 p2 = texelFetch(vertices, tri.y & 0xffff).xyz;

				// Plane intersection.
				vec3 n = cross(p1 - p0, p2 - p0);
				float t = dot(p0 - ro, n) / dot(rd, n);

				// Signed areas of subtriangles.
				float d0 = dot(rd, cross(p0 - ro, p1 - ro));
				float d1 = dot(rd, cross(p1 - ro, p2 - ro));
				float d2 = dot(rd, cross(p2 - ro, p0 - ro));

				// The ray intersects the triangle only if the subtriangles all have the same sign and
				// the ray intersects the triangle's plane.
				if (sign(d0) == sign(d1) && sign(d0) == sign(d2) && t < maxdist && t > 0.0)
					return false;
			}
			
			// Step to the immediate next node.
			++node;
		}
		else
		{
			// The ray does not intersect this node, so skip over all the nodes contained within it.
			node = n0.w;
		}
		
	} while (node > 0);
#endif

	// There is no intersection detected.
	return true;
}

// Returns the closest distance along the given ray at which the ray intersects a solid leaf in the
// BSP, or returns MAXT if there was no intersection.
float traceRayBSP(vec3 org, vec3 dir, float t0, float max_t)
{
	// The 'other' variables represent a one-level stack.
	vec2 other_node = vec2(0);
	float other_t1 = max_t;
	vec4 other_pln0;

	// Continue visiting nodes until the next nearest plane lies beyond max_t (in which case
	// there can be no more intersections possible).
	while (t0 < max_t)
	{
		// Pop from the stack.
		vec2  node = other_node;
		float t1 = other_t1;
		vec4 pln0 = other_pln0;
		
		// Reset the stack.
		other_node = vec2(0);
		other_t1 = max_t;

		// Travel down the BSP.
		do
		{
			vec4 pln = texture(bsp_planes, node);
			vec4 children = texture(bsp_branches, node);
			
			// Perform ray-plane intersection and order the children according to traversal order.
			
			float t = dot(pln.xyz, dir);

			children = (t > 0.0) ? children.zwxy : children.xyzw;

			t = (pln.w - dot(pln.xyz, org)) / t;

			if (t > t0)
			{
				// The near node is intersected.
				node = children.xy;
				
				if (t < t1) 
				{
					// Both children need to be visited, so push the further children onto the stack.
					other_t1 = t1;
					t1 = t;
					other_pln0 = pln0;
					pln0 = pln;
					other_node = children.zw;
				}
			}
			else
			{
				// Only the far node is intersected.
				node = children.zw;
			}
			
			if (node.y == 1.0)
			{
				 return t0;
			}
			
		} while (node != vec2(0));
		
		// Store the plane of intersection and push the current ray traversal point just beyond it.
		out_pln = pln0;
		t0 = t1 + EPS;
	}

	return MAXT;
}

// Returns false if the given ray intersects a solid leaf in the BSP and the distance
// to the intersection along the ray is less than maxdist, otherwise returns true.
bool traceRayShadowBSP(vec3 org, vec3 dir, float t0, float max_t)
{
	return traceRayBSP(org, dir, t0, max_t) >= max_t;
}

// Returns the light list head references for the given point. The X component points to the 
// direct light list and the Y component points to the skyportal list.
ivec2 getLightRef(vec3 p)
{
	vec2 node = vec2(0), prev_node;
	float d;
	
	// Travel down the BSP.
	do
	{
		vec4 pln = texture(bsp_planes, node);
		vec4 children = texture(bsp_branches, node);
		
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

// Returns the amount of direct light incident at the given point with the given surface normal.
// oli is an index to the first lightsource reference.
vec3 sampleDirectLight(vec3 rp, vec3 rn, int oli)
{
	vec3 r = vec3(0);
	oli = oli < 0 ? -oli : +oli;
	int li = oli;
	int ref = texelFetch(lightrefs, li).r;
	
	if (ref != -1)
	{
		float wsum = 0.0;
		
		// Visit each referenced light and sum up the importance weights for the purpose of weight normalisation.
		do
		{
			vec4 light = texelFetch(lights, ref);

			ivec2 tri = texelFetch(triangles, floatBitsToInt(light.w)).xy;

			vec3 p0 = texelFetch(tri_vertices, tri.x & 0xffff).xyz;
			vec3 p1 = texelFetch(tri_vertices, tri.x >> 16).xyz;
			vec3 p2 = texelFetch(tri_vertices, tri.y & 0xffff).xyz;

			vec3 n = cross(p2 - p0, p1 - p0);

			// Calculate the importance weight.
			float d = distance(rp, (p0 + p1 + p2) / 3.0);
			float pd = dot(rp - p0, n);
			float w = length(light.rgb) * 1.0 / (d * d) * sqrt(max(0.0, pd));

			wsum += w;
			++li;
			ref = texelFetch(lightrefs, li).r;
			
		} while (ref != -1);
		
		// Sample the set of lightsources using importance sampling.
		for (int light_sample = 0; light_sample < NUM_LIGHT_SAMPLES; ++light_sample)
		{
			li = oli;
			ref = texelFetch(lightrefs, li).r;
			
			vec3 p0, p1, p2, n;

			// Choose a lightsource randomly, with each lightsource having a probability of being chosen
			// proportional to it's importance weight.
			
			float x = rand(light_sample).x * wsum, w=1.0;
			vec4 j = vec4(0);

			do
			{
				vec4 light=texelFetch(lights, ref);
				ivec2 tri = texelFetch(triangles, floatBitsToInt(light.w)).xy;
				
				p0 = texelFetch(tri_vertices, tri.x & 0xffff).xyz;
				p1 = texelFetch(tri_vertices, tri.x >> 16).xyz;
				p2 = texelFetch(tri_vertices, tri.y & 0xffff).xyz;
				
				n = cross(p2 - p0, p1 - p0);
				
				// Calculate the importance weight.
				float d = distance(rp, (p0 + p1 + p2) / 3.);
				float pd = dot(rp - p0, n);
				
				w = length(light.rgb) * 1.0 / (d * d) * sqrt(max(0.0, pd));	
				
				x -= w;
				
				if (x < 0.0)
				{
					// This lightsource has been chosen.
					j = light;
					break;
				}
				
				++li;
				ref = texelFetch(lightrefs, li).r;
				
			} while (ref != -1);

#if NUM_SHADOW_SAMPLES > 0
			// Trace shadow rays towards the lightsource to calculate a shadowing term.
			for (int shadow_sample = 0; shadow_sample < NUM_SHADOW_SAMPLES; ++shadow_sample)
			{
				vec3 sp = rp;
				vec3 sn = rn.xyz;
				vec3 lp = p0;
				vec2 uv = rand(shadow_sample);
				
				// Flip the barycentric coordinates if this is a triangle, don't if it's a parallelogram.
				if ((uv.x + uv.y) > 1.0 && dot(j.rgb, vec3(1)) > 0.0)
					uv = vec2(1) - uv;
				
				lp += (p1 - p0) * uv.x + (p2 - p0) * uv.y;
				
				float ld = distance(sp, lp);
				
				vec3 l = (lp - sp) / ld;
				float ndotl = dot(l, sn), lndotl = dot(-l, n);
				if (ndotl > 0.0 && lndotl > 0.0)
				{
					float s = (traceRayShadowBSP(sp, l, EPS * 16, ld) && traceRayShadowTri(sp, l, ld, tri_nodes0, tri_nodes1, tri_vertices, triangles)) ? 1.0 / (ld * ld) : 0.0;
					r += s * ndotl * lndotl * abs(j.rgb) * wsum / w;
				}
			}
#else
			r += abs(j.rgb) * wsum / w;
#endif
		}
	}

	return r / float(NUM_LIGHT_SAMPLES * NUM_SHADOW_SAMPLES);
}

float bumpHeightForDiffuseTexel(vec2 st)
{
	return dot(vec3(1.0 / 3.0), texture(diffuse_texture, st).rgb);
}

void main()
{
	vec4 pln;

	gl_FragColor = vec4(0);

	// Get the ray origin point.
	vec3 rp = texcoords[1].xyz + texcoords[3].xyz * EPS * 16;

	// Get the surface normal of the surface being rasterised.
	pln.xyz = normalize(texcoords[3].xyz);
	pln.w = dot(rp, pln.xyz);

	// Construct a shading normal for bump-mapping.
	vec2 bump_texel_size = vec2(1) / textureSize(diffuse_texture, 0).xy;

	float bump_height0 = bumpHeightForDiffuseTexel(texcoords[0].st);

	vec2 bump_gradients = vec2(bumpHeightForDiffuseTexel(texcoords[0].st + vec2(bump_texel_size.x, 0.0)) - bump_height0,
										bumpHeightForDiffuseTexel(texcoords[0].st + vec2(0.0, bump_texel_size.y)) - bump_height0) / bump_texel_size;
										
	vec3 shading_normal = normalize(pln.xyz - (texcoords[5].xyz * bump_gradients.x + texcoords[6].xyz * bump_gradients.y) * bump_factor);
	
	// Sample the direct light at this point.
	int rpli = getLightRef(rp).x;
	vec3 r = sampleDirectLight(rp, shading_normal, rpli);

	// Add the emissive light (light emitted towards the eye immediately from this surface).
	r += texcoords[4].rgb;
	
#if NUM_BOUNCES > 0 || NUM_AO_SAMPLES > 0 || NUM_SKY_SAMPLES > 0
	// Make an orthonormal basis for the primary ray intersection point.
	vec3 u = normalize(cross(pln.xyz, pln.zxy));
	vec3 v = cross(pln.xyz, u);
#endif
	
#if NUM_AO_SAMPLES > 0
	{
		// Apply importance-sampled (lambert diffuse BRDF) ambient occlusion.
		float ao = 0.0;
		for (int ao_sample = 0; ao_sample < NUM_AO_SAMPLES; ++ao_sample)
		{
			vec2 rr = rand(ao_sample);
			
			float r1 = 2.0 * PI * rr.x;
			float r2s = sqrt(rr.y);

			// Lambert diffuse BRDF.
			vec3 rd = u * cos(r1) * r2s + v * sin(r1) * r2s + pln.xyz * sqrt(1.0 - rr.y);
				
			if (traceRayShadowBSP(rp, rd, EPS * 16, ao_radius) && traceRayShadowTri(rp, rd, ao_radius, tri_nodes0, tri_nodes1, tri_vertices, triangles))
				ao += 1.0;
		}
		gl_FragColor.rgb += ao_color * ao / float(NUM_AO_SAMPLES);
	}
#endif

#if NUM_BOUNCES > 0 || NUM_SKY_SAMPLES > 0

	vec3 sp, rd;
	float t;
	float factor = bounce_factor;

#if NUM_BOUNCES == 0
	// If there are no secondary bounces and there are no visible skyportals then there is no
	// need to sample the BRDF and trace a ray with it.
	if (rpli < 0)
#endif
	{
		float fresnel = texcoords[5].w * mix(1.0, 0.5, pow(clamp(1.0 - dot(shading_normal, -normalize(texcoords[7].xyz)), 0.0, 1.0), 5.0));
		if (bluenoise_sample.b < fresnel)
		{
			// Lambert diffuse BRDF.
			vec2 rr = rand(0);
			float r1 = 2.0 * PI * rr.x;
			float r2s = sqrt(rr.y);
			rd = normalize(u * cos(r1) * r2s + v * sin(r1) * r2s + pln.xyz * sqrt(1.0 - rr.y));
			factor = clamp(dot(rd, shading_normal) / dot(rd, pln.xyz), 0.1, 2.0);
		}
		else
		{
			// Specular reflection.
			rd = normalize(reflect(normalize(texcoords[7].xyz), shading_normal));
		}
		
		// Trace a ray against the BSP. This intersection point is later used for secondary bounces and testing
		// for containment in skyportals polygons (skyportals aren't sampled directly).
		t = traceRayBSP(rp, rd, EPS * 16, MAXT) - 1.0;

		if ((dot(out_pln.xyz, rp) - out_pln.w) < 0.0)
			out_pln *= -1.0;
		
		sp = rp + rd * max(0.0, t);
	}
	
#if NUM_BOUNCES > 0
	// Test for intersection with triangle meshes. This is done so bounce light can be shadowed
	// by triangle meshes.
	if (traceRayShadowTri(rp, rd, t, tri_nodes0, tri_nodes1, tri_vertices, triangles))
	{
		int li = getLightRef(sp).x;
		r += factor * sampleDirectLight(sp, out_pln.xyz, li);
	}
#endif

#if NUM_SKY_SAMPLES > 0
	if (rpli < 0)
	{
		// The list of skyportals is non-empty, so trace rays by sampling the BRDF and test for containment
		// of the intersection points from those rays in skyportal polygons.
		vec3 sky_r = vec3(0);
		for (int sky_sample = 0; sky_sample < NUM_SKY_SAMPLES; ++sky_sample)
		{
			int li = getLightRef(sp).y;
			int ref = texelFetch(lightrefs, li).r;
			if (ref != -1)
			{
				// Test for intersection with any triangle meshes. This is done so triangle meshes can cast shadows from sky light.
				if (traceRayShadowTri(rp, rd, t, tri_nodes0, tri_nodes1, tri_vertices, triangles))
				{
					// Visit each skyportal in this cluster and test for containment.
					do
					{
						vec4 light = texelFetch(lights, ref);
						ivec2 tri = texelFetch(triangles, floatBitsToInt(light.w)).xy;
						
						vec3 p0 = texelFetch(tri_vertices, tri.x & 0xffff).xyz;
						vec3 p1 = texelFetch(tri_vertices, tri.x >> 16).xyz;
						vec3 p2 = texelFetch(tri_vertices, tri.y & 0xffff).xyz;
						
						vec3 n = normalize(cross(p1 - p0, p2 - p0));
						
						vec3 sp2 = sp;
						
						float s1 = dot(cross(p1 - sp2, p2 - sp2), n);

						// If the skyportal is a parallelogram and the subtriangle opposing the reflected
						// vertex has negative area then reflect the sample point back into the triangle.
						if (dot(light.rgb, vec3(1)) < 0.0 && s1 < 0.0)
						{
							s1 *= -1.0;
							vec3 mirror = normalize(p2 - p1);
							sp2 -= 2.0 * mirror * dot(sp2 - (p1 + p2) * 0.5, mirror);
						}

						float s0 = dot(cross(p0 - sp2, p1 - sp2), n);
						float s2 = dot(cross(p2 - sp2, p0 - sp2), n);

						if (s0 > 0.0 && s1 > 0.0 && s2 > 0.0 && abs(dot(n, sp2 - p0)) < 1.0)
						{
							// The sample point is contained by this polygon, so add the light contribution.
							sky_r += abs(light.rgb) * clamp(dot(rd, shading_normal) / dot(rd, pln.xyz), 0.1, 2.0);
						}
						
						++li;
						ref = texelFetch(lightrefs, li).r;
						
					} while (ref != -1);
				}
			}

#if NUM_SKY_SAMPLES > 1
			// There are further sky samples to be made, so sample the BRDF again to trace a new ray
			// out into the scene to create a new sample point.
			vec2 rr = rand(sky_sample + 1);

			float r1 = 2.0 * PI * rr.x;
			float r2s = sqrt(rr.y);

			// Lambert diffuse BRDF.
			rd = normalize(u * cos(r1) * r2s + v * sin(r1) * r2s + pln.xyz * sqrt(1.0 - rr.y));

			t = traceRayBSP(rp, rd, EPS * 16, MAXT) - 1.0;
			
			sp = rp + rd * max(0.0, t);
#endif
		}
		r += sky_r / float(NUM_SKY_SAMPLES);
	}
#endif

#if NUM_BOUNCES > 1
	{
		if ((dot(out_pln.xyz, rp) - out_pln.w) < 0.0)
			out_pln *= -1.0;

		// Trace secondary rays to gather bounce light from surrounding surfaces. This does not include sky light.
		float factor = bounce_factor;
		rp = sp;
		for (int bounce = 1; bounce < NUM_BOUNCES; ++bounce)
		{
			vec2 rr = rand(bounce);

			float r1 = 2.0 * PI * rr.x;
			float r2s = sqrt(rr.y);

			// Lambert diffuse BRDF.
			vec3 rd = normalize(u * cos(r1) * r2s + v * sin(r1) * r2s + out_pln.xyz * sqrt(1.0 - rr.y));

			t = traceRayBSP(rp, rd, EPS * 16, MAXT) - 1.0;

			if (!traceRayShadowTri(rp, rd, t, tri_nodes0, tri_nodes1, tri_vertices, triangles))
				break;
			
			if ((dot(out_pln.xyz, rp) - out_pln.w) < 0.0)
				out_pln *= -1.0;
			
			rp = rp + rd * max(0.0, t);
			
			// Simulate (achromatic) absorption of light at the surface.
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

	// Apply an arbitrary scaling to empirically match the output of qrad3.
	gl_FragColor.rgb += r / 1024.;

	gl_FragColor.a = color.a;
	
#if DIFFUSE_MAP_ENABLE
	// Multiply the gathered light with the diffuse albedo.
	gl_FragColor.rgb *= texture(diffuse_texture, texcoords[0].st).rgb + vec3(1e-2);
#endif

	// Tone mapping (Simple Reinhard).
	gl_FragColor.rgb *= exposure / (vec3(1) + gl_FragColor.rgb / exposure);

	// Gamma.
	gl_FragColor.rgb = pow(gl_FragColor.rgb, vec3(1.0 / gamma));
	
#if TAA_ENABLE
	if (texcoords[4].w > 0.0)
	{
		// Apply TAA.
		
		vec4 clip = previous_world_matrix * texcoords[1];
		vec2 ndc = clip.xy / clip.w * 0.5 + vec2(0.5);
		
		rp = texcoords[1].xyz + texcoords[3].xyz * EPS * 16;
		
		// Check if the fragment was visible in the previous frame. If not, then the information in the previous framebuffer does not match and it cannot be reused.
		vec3 disocclusion_test_ray = normalize(previous_view_origin - rp);
		float disocclusion_test_distance = distance(rp, previous_view_origin);
		bool previously_visible = traceRayShadowBSP(rp, disocclusion_test_ray, EPS * 16, disocclusion_test_distance) &&
				traceRayShadowTri(rp, disocclusion_test_ray, disocclusion_test_distance, tri_nodes0_prev, tri_nodes1_prev, tri_vertices_prev, triangles_prev) &&
				dot(normalize(texcoords[3].xyz), disocclusion_test_ray) > 0.01;
		
		gl_FragColor.rgb = mix(gl_FragColor.rgb, texture(taa_world, ndc).rgb,
				all(greaterThan(ndc, vec2(0))) && all(lessThan(ndc, vec2(1))) && previously_visible ? 0.45 * texcoords[4].w : 0.0);
	}
#endif
}
