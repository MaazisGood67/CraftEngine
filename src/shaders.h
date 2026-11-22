// Embedded HLSL (Shader Model 4.0, the DX10 target).
#pragma once

static const char* kTerrainShader = R"HLSL(
cbuffer CB : register(b0)
{
    row_major float4x4 gViewProj;
    float4 gCamPos;
    float4 gFogColor;
    float4 gFogParams; // x = fog start, y = fog end
};

struct VSIn
{
    float3 pos   : POSITION;
    float2 uv    : TEXCOORD0;
    float  tile  : TEXCOORD1;
    float  light : TEXCOORD2;
};

struct VSOut
{
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float  tile  : TEXCOORD1;
    float  light : TEXCOORD2;
    float  dist  : TEXCOORD3;
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    o.pos   = mul(float4(i.pos, 1.0f), gViewProj);
    o.uv    = i.uv;
    o.tile  = i.tile;
    o.light = i.light;
    o.dist  = distance(i.pos, gCamPos.xyz);
    return o;
}

Texture2DArray gTex  : register(t0);
SamplerState   gSamp : register(s0);

float4 PSMain(VSOut i) : SV_Target
{
    float  slice  = floor(i.tile + 0.5f);
    float4 texel  = gTex.SampleLevel(gSamp, float3(frac(i.uv), slice), 0);
    if (texel.a < 0.5f) discard; // leaves / cutout blocks
    float3 color  = texel.rgb * i.light;

    float fog = saturate((i.dist - gFogParams.x) / max(gFogParams.y - gFogParams.x, 1e-3f));
    color = lerp(color, gFogColor.rgb, fog);

    return float4(color, 1.0f);
}
)HLSL";

// Simple untextured coloured-vertex shader used for mobs/projectiles.
static const char* kEntityShader = R"HLSL(
cbuffer CB : register(b0)
{
    row_major float4x4 gViewProj;
};
struct EIn
{
    float3 pos : POSITION;
    float4 col : COLOR0;
};
struct EOut
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
};
EOut VSEntity(EIn i)
{
    EOut o;
    o.pos = mul(float4(i.pos, 1.0f), gViewProj);
    o.col = i.col;
    return o;
}
float4 PSEntity(EOut i) : SV_Target
{
    return i.col;
}
)HLSL";

static const char* kGuiShader = R"HLSL(
cbuffer CB : register(b0)
{
    float2 gScreen;
    float2 _pad;
};

struct VSIn
{
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

VSOut VSGui(VSIn i)
{
    VSOut o;
    o.pos = float4(i.pos.x / gScreen.x * 2.0f - 1.0f, 1.0f - i.pos.y / gScreen.y * 2.0f, 0.0f, 1.0f);
    o.uv = i.uv;
    o.col = i.col;
    return o;
}

Texture2D    gTex  : register(t0);
SamplerState gSamp : register(s0);

float4 PSGui(VSOut i) : SV_Target
{
    return gTex.Sample(gSamp, i.uv) * i.col;
}
)HLSL";

static const char* kPanoramaShader = R"HLSL(
cbuffer CB : register(b0)
{
    float2 gScreen;
    float  gTanHalf;
    float  gAspect;
    float  gYaw;
    float  gPitch;
    float2 _pad;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 ndc : TEXCOORD0;
};

VSOut VSPano(uint id : SV_VertexID)
{
    float2 p = float2((id << 1) & 2, id & 2);
    p = p * 2.0f - 1.0f;
    VSOut o;
    o.pos = float4(p, 1.0f, 1.0f);
    o.ndc = p;
    return o;
}

Texture2D    face0 : register(t0);
Texture2D    face1 : register(t1);
Texture2D    face2 : register(t2);
Texture2D    face3 : register(t3);
Texture2D    face4 : register(t4);
Texture2D    face5 : register(t5);
SamplerState gSamp : register(s0);

float4 PSPano(VSOut i) : SV_Target
{
    float3 d = normalize(float3(i.ndc.x * gTanHalf * gAspect, -i.ndc.y * gTanHalf, 1.0f));

    float cp = cos(gPitch), sp = sin(gPitch);
    d = float3(d.x, d.y * cp - d.z * sp, d.y * sp + d.z * cp);
    float cy = cos(gYaw), sy = sin(gYaw);
    d = float3(d.x * cy + d.z * sy, d.y, -d.x * sy + d.z * cy);

    float ax = abs(d.x), ay = abs(d.y), az = abs(d.z);
    float2 uv;
    int face;
    if (ax >= ay && ax >= az)
    {
        float3 a = d / ax;
        if (d.x > 0.0f) { face = 3; uv = float2(-a.z, -a.y); }
        else            { face = 1; uv = float2( a.z, -a.y); }
    }
    else if (ay >= az)
    {
        float3 a = d / ay;
        if (d.y > 0.0f) { face = 4; uv = float2(a.x,  a.z); }
        else            { face = 5; uv = float2(a.x, -a.z); }
    }
    else
    {
        float3 a = d / az;
        if (d.z > 0.0f) { face = 0; uv = float2( a.x, -a.y); }
        else            { face = 2; uv = float2(-a.x, -a.y); }
    }

    uv = uv * 0.5f + 0.5f;
    uv.y = 1.0f - uv.y;

    float3 c;
    if (face == 0)      c = face0.SampleLevel(gSamp, uv, 0).rgb;
    else if (face == 1) c = face1.SampleLevel(gSamp, uv, 0).rgb;
    else if (face == 2) c = face2.SampleLevel(gSamp, uv, 0).rgb;
    else if (face == 3) c = face3.SampleLevel(gSamp, uv, 0).rgb;
    else if (face == 4) c = face4.SampleLevel(gSamp, uv, 0).rgb;
    else                c = face5.SampleLevel(gSamp, uv, 0).rgb;
    return float4(c, 1.0f);
}
)HLSL";
