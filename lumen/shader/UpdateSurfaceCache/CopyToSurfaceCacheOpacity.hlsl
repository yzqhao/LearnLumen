#include "../StaticSample.hlsli"

ByteAddressBuffer RectCoordBuffer : register(t0);
ByteAddressBuffer RectUVBuffer : register(t1);
int2 select_internal(bool2   c, int2 a, int2 b) { return int2(c.x ? a.x : b.x, c.y ? a.y : b.y); }
float2 select_internal(bool2   c, float2 a, float2 b) { return float2(c.x ? a.x : b.x, c.y ? a.y : b.y); }                       
bool2 VertMax(uint VertexId)
{
	bool2 bVertMax;
	bVertMax.x = VertexId == 1 || VertexId == 2 || VertexId == 4;
	bVertMax.y = VertexId == 2 || VertexId == 4 || VertexId == 5;
	return bVertMax;
}

uint4 Decode(ByteAddressBuffer buf, uint InstanceId)
{
    uint RectCoord0 = buf.Load(InstanceId * 16u);
    uint RectCoord1 = buf.Load(InstanceId * 16u + 4);
    uint RectCoord2 = buf.Load(InstanceId * 16u + 8);
    uint RectCoord3 = buf.Load(InstanceId * 16u + 12);
    uint4 RectCoord = uint4(RectCoord0, RectCoord1, RectCoord2, RectCoord3);
    return RectCoord;
}

void VS(
	in uint InstanceId : SV_InstanceID,
	in uint VertexId : SV_VertexID,
	out float4 OutPosition : SV_POSITION,
	out float2 OutUV : TEXCOORD0,
	out float2 OutRectUV : TEXCOORD1,
	out float OutRectIndex : RECT_INDEX)
{
	float DownsampleFactor=1.0f;
	float2 InvTextureSize=float2(0.0019531f,0.0019531f);
	float2 InvViewSize=float2(0.0002441f,0.0002441f);
    uint4 RectCoord = Decode(RectCoordBuffer, InstanceId) * DownsampleFactor;
	uint2 VertexCoord =  select_internal( VertMax(VertexId) , RectCoord.zw , RectCoord.xy );
	float4 RectUV = RectCoord * InvViewSize.xyxy;
	{
        RectUV = Decode(RectUVBuffer, InstanceId) * DownsampleFactor * InvTextureSize.xyxy;
    }
	float2 VertexUV =  select_internal( VertMax(VertexId) , RectUV.zw , RectUV.xy );
	OutPosition = float4(float2(VertexCoord) * InvViewSize * float2(2.0f, -2.0f) + float2(-1.0, 1.0f), 0.0f, 1.0f);
	OutUV = VertexUV;
	OutRectUV.x = VertMax(VertexId).x ? 1.0f : 0.0f;
	OutRectUV.y = VertMax(VertexId).y ? 1.0f : 0.0f;
	OutRectIndex = InstanceId;
}
//(0.0f,0.0f,1.0f) => (0.3,0.4,0.4) => 0.3*0.3+0.4*0.4+0.4*0.4 != 1.0

float4 Texture2DSampleLevel(Texture2D Tex, SamplerState Sampler, float2 UV, float Mip)
{
	return Tex.SampleLevel(Sampler, UV, Mip);
}
Texture2D SourceAlbedoAtlas:register(t2);

void PS(
    float4 Position : SV_POSITION,
	float2 AtlasUV : TEXCOORD0,
	out float4 OutColor0 : SV_Target0
)
{
	uint2 WriteCoord = (uint2) Position.xy;
	{
		{
            float Opacity = Texture2DSampleLevel(SourceAlbedoAtlas, gsamPointClamp, AtlasUV, 0).w;
			OutColor0 = float4(Opacity, 0.0f, 0.0f, 0.0f);
		}
	}
}