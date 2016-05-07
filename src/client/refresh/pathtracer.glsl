#version 330

#define EPS	 (1./32.)
#define MAXT	(2048.)

in vec4 texcoords[5], color;

uniform sampler2D tex0;
uniform sampler2D planes;
uniform sampler2D branches;

uniform int frame = 0;

float pi = acos(-1.);
float aoradius = 150.;
float seed = 0.;
float rand() { return fract(sin(seed++)*43758.5453123); }

vec3 rp = texcoords[1].xyz;
vec3 dir = normalize(texcoords[2].xyz);
vec3 normal = texcoords[3].xyz;
vec4 out_pln;



uniform isamplerBuffer node0;
uniform isamplerBuffer node1;
uniform samplerBuffer edge0;
uniform isamplerBuffer triangle;

uniform samplerBuffer lights;
uniform isamplerBuffer lightrefs;
uniform isampler2D bsp_lightrefs;

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

} while(node > 0);

return true;
}
bool traceRayShadowBSP(vec3 org, vec3 dir, float t0, float max_t)
{
vec2  other_node=vec2(0);
float other_t1=max_t;

while(t0<max_t)
{
	vec2  node=other_node;
	float t1=other_t1;
	
	other_node=vec2(0);
	other_t1=max_t;

	do{
		vec4 pln=texture(planes,node);
		vec4 children=texture(branches,node);
		
		float t=dot(pln.xyz,dir);

		children=(t>0.0) ? children.zwxy : children.xyzw;

		t=(pln.w-dot(pln.xyz,org)) / t;

		if(t>t0)
		{
			node=children.xy;
			
			if(t<t1) 
			{
				other_t1=t1;
				t1=t;
				other_node=children.zw;
			}
		}
		else
		{
			node=children.zw;
		}
		
		if(node.y==1.0)
		{
			 return false;
		}
		
	} while(node!=vec2(0.0));
	
	t0=t1+EPS;
}

return true;
}

float traceRayBSP(vec3 org, vec3 dir, float t0, float max_t)
{
vec2  other_node=vec2(0);
float other_t1=max_t;
vec4 other_pln0;

while(t0<max_t)
{
	vec2  node=other_node;
	float t1=other_t1;
	vec4 pln0=other_pln0;
	
	other_node=vec2(0);
	other_t1=max_t;

	do{
		vec4 pln=texture(planes,node);
		vec4 children=texture(branches,node);
		
		float t=dot(pln.xyz,dir);

		children=(t>0.0) ? children.zwxy : children.xyzw;

		t=(pln.w-dot(pln.xyz,org)) / t;

		if(t>t0)
		{
			node=children.xy;
			
			if(t<t1) 
			{
				other_t1=t1;
				t1=t;
				other_pln0=pln0;
				pln0=pln;
				other_node=children.zw;
			}
		}
		else
		{
			node=children.zw;
		}
		
		if(node.y==1.0)
		{
			 return t0;
		}
		
	} while(node!=vec2(0.0));
	
	out_pln=pln0;
	t0=t1+EPS;
}

return 1e8;
}



int getLightRef(vec3 p)
{
int index=0;
vec2  node=vec2(0);
	do{
		vec4 pln=texture(planes,node);
		vec4 children=texture(branches,node);
		ivec2 light_indices = texture(bsp_lightrefs, node).rg;
		
		float d=dot(pln.xyz,p)-pln.w;
		if(d<0.0) { node = children.zw; index = light_indices.y; } else { node = children.xy; index = light_indices.x; };

		if(node.y==1.0)
		{
			 return -1;
		}
		
	} while(node!=vec2(0.0));
return index;
}


int sky_li;
vec3 sampleDirectLight(vec3 rp, vec3 rn)
{
vec3 r=vec3(0);
int oli=getLightRef(rp),li=oli;
int ref=texelFetch(lightrefs, li).r;
float wsum=0.;
if(ref != -1) do{
			vec4 light=texelFetch(lights, ref);
			ivec2 tri = texelFetch(triangle, floatBitsToInt(light.w)).xy;
			vec3 p0 = texelFetch(edge0, tri.x & 0xffff).xyz;
			vec3 p1 = texelFetch(edge0, tri.x >> 16).xyz;
			vec3 p2 = texelFetch(edge0, tri.y).xyz;
			vec3 n = cross(p2 - p0, p1 - p0);
			float d=distance(rp,(p0+p1+p2)/3.);
			float pd=dot(rp-p0,n);
			float w=length(light.rgb)*1./(d*d)*sqrt(max(0.,pd));
			wsum+=w;
			++li;
			ref=texelFetch(lightrefs, li).r;
} while(ref!=-1);

sky_li = li;
float x=rand()*wsum,w=1;
vec4 j=vec4(0);

li=oli;
ref=texelFetch(lightrefs, li).r;
vec3 p0,p1,p2,n;
if(ref != -1) 	do{
			vec4 light=texelFetch(lights, ref);
			ivec2 tri = texelFetch(triangle, floatBitsToInt(light.w)).xy;
			p0 = texelFetch(edge0, tri.x & 0xffff).xyz;
			p1 = texelFetch(edge0, tri.x >> 16).xyz;
			p2 = texelFetch(edge0, tri.y).xyz;
			n = cross(p2 - p0, p1 - p0);
			float d=distance(rp,(p0+p1+p2)/3.);
			float pd=dot(rp-p0,n);
			w=length(light.rgb)*1./(d*d)*sqrt(max(0.,pd));	
	x-=w;
	if(x<0.)
	{
		j=light;
		break;
	}
			++li;
			ref=texelFetch(lightrefs, li).r;
	} while(ref!=-1);

vec4 light=j;
vec3 sp=rp;
vec3 sn=rn.xyz;
vec3 lp=p0;
 vec2 uv = vec2(rand(), rand());
 if((uv.x + uv.y) > 1. && dot(light.rgb,vec3(1)) > 0.0) uv = vec2(1) - uv;
lp+=(p1 - p0)*uv.x+(p2 - p0)*uv.y;
float ld=distance(sp,lp);
vec3 l=(lp-sp)/ld;
float ndotl=dot(l,sn),lndotl=dot(-l,n);
if(ndotl>0. && lndotl>0.)
{
	float s=(traceRayShadowBSP(sp,l,EPS*16,min(2048.,ld)) && traceRayShadowTri(sp,l,min(2048.,ld))) ? 1./(ld*ld) : 0.;
	r+=s*ndotl*lndotl*abs(light.rgb)/(w/wsum);
}
return r;
}

void main()
{
seed = gl_FragCoord.x * 89.9 + gl_FragCoord.y * 197.3 + frame*0.02;

rp+=normal*EPS*16;

out_pln.xyz=normal;
out_pln.w=dot(rp,out_pln.xyz);

vec4 spln=out_pln;

// permutate components
vec3 b=vec3(spln.z,spln.x,spln.y);

// tangents
vec3 u=normalize(cross(spln.xyz,b)),
		 v=cross(spln.xyz,u);
  
vec3 r=texcoords[4].rgb;
  

r+=sampleDirectLight(rp, spln.xyz);

#if 1
		float r1=2.0*pi*rand();
		float r2=rand();
		float r2s=sqrt(r2);

		vec3 rd=normalize(u*cos(r1)*r2s + v*sin(r1)*r2s + spln.xyz*sqrt(1.0-r2));
			
		float t=traceRayBSP(rp,rd,EPS*16,2048.);

if ((dot(out_pln.xyz,rp) - out_pln.w) < 0.0) out_pln *= -1.0;
 r+=.75*sampleDirectLight(rp+rd*max(0.0, t - 1.0), out_pln.xyz);


/* Sky portals */

int li=sky_li;
++li;
int ref=texelFetch(lightrefs, li).r;
if(ref != -1){

if(traceRayShadowTri(rp,rd,t))
	do{
			vec3 sp=rp+rd*max(0.0, t - 1.0);
			vec4 light=texelFetch(lights, ref);
			ivec2 tri = texelFetch(triangle, floatBitsToInt(light.w)).xy;
			vec3 p0 = texelFetch(edge0, tri.x & 0xffff).xyz;
			vec3 p1 = texelFetch(edge0, tri.x >> 16).xyz;
			vec3 p2 = texelFetch(edge0, tri.y).xyz;
			vec3 n = normalize(cross(p2 - p0, p1 - p0));
			if(dot(light.rgb, vec3(1)) < 0.0)
			{
				vec3 mirror = normalize(cross(p2 - p1, n));
				sp -= 2.0 * mirror * dot(sp - p1, mirror);
			}
			float s0 = dot(cross(p0 - sp, p1 - sp), n);
			float s1 = dot(cross(p1 - sp, p2 - sp), n);
			float s2 = dot(cross(p2 - sp, p0 - sp), n);

			if (s0 < 0.0 && s1 < 0.0 && s2 < 0.0 && abs(dot(n, sp - p0)) < 1.)
				r += light.rgb;
			++li;
			ref=texelFetch(lightrefs, li).r;
	} while(ref!=-1);
}

#endif

//	}
gl_FragColor.rgb = r / 1024.;

/*	This code implements ambient occlusion. It has been temporarily disabled.
vec3 r=vec3(0.0);
const int n=4;

for(int i=0;i<n;++i)
{
		float r1=2.0*pi*rand();
		float r2=rand();
		float r2s=sqrt(r2);

		vec3 rd=u*cos(r1)*r2s + v*sin(r1)*r2s + spln.xyz*sqrt(1.0-r2);
			
		if(traceRayShadowBSP(rp,rd,EPS*16,aoradius) && traceRayShadowTri(rp,rd,aoradius))
			r+=vec3(1.0/float(n));
}

gl_FragColor.rgb = r;
gl_FragColor.a = 1.0;
*/

gl_FragColor.a = color.a;
gl_FragColor.rgb *= texture(tex0, texcoords[0].st).rgb + vec3(1e-2);
gl_FragColor.rgb = sqrt(gl_FragColor.rgb);
}
