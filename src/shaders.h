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
    float3 albedo = gTex.SampleLevel(gSamp, float3(frac(i.uv), slice), 0).rgb;
    float3 color  = albedo * i.light;

    float fog = saturate((i.dist - gFogParams.x) / max(gFogParams.y - gFogParams.x, 1e-3f));
    color = lerp(color, gFogColor.rgb, fog);

    return float4(color, 1.0f);
}
)HLSL";
