ByteAddressBuffer RectCoordBuffer : register(t0);

int2 select_internal(bool2 c, int2 a, int2 b)
{
    return int2(c.x ? a.x : b.x, c.y ? a.y : b.y);
}

bool2 VertMax(uint VertexId)
{
    bool2 bVertMax;
    bVertMax.x = VertexId == 1 || VertexId == 2 || VertexId == 4;
    bVertMax.y = VertexId == 2 || VertexId == 4 || VertexId == 5;
    return bVertMax;
}

//render target -> rect/sprite
// RasterizeToRects
void VS(
	in uint InstanceId : SV_InstanceID, //0~11
	in uint VertexId : SV_VertexID, //0~5
	out float4 OutPosition : SV_POSITION)
{
    float2 InvViewSize = float2(0.0019531f, 0.0019531f);    //0.0019531f = 1 / 512
    uint RectCoord0 = RectCoordBuffer.Load(InstanceId * 16u);
    uint RectCoord1 = RectCoordBuffer.Load(InstanceId * 16u + 4);
    uint RectCoord2 = RectCoordBuffer.Load(InstanceId * 16u + 8);
    uint RectCoord3 = RectCoordBuffer.Load(InstanceId * 16u + 12);
    uint4 RectCoord = uint4(RectCoord0, RectCoord1, RectCoord2, RectCoord3);
    //RectCoordBuffer[ InstanceId];
    uint2 VertexCoord = select_internal(VertMax(VertexId), RectCoord.zw, RectCoord.xy);
    OutPosition = float4(float2(VertexCoord) * InvViewSize * float2(2.0f, -2.0f) + float2(-1.0, 1.0f), 0.0f, 1.0f);
}

//(0.0f,0.0f,1.0f) => (0.3,0.4,0.4) => 0.3*0.3+0.4*0.4+0.4*0.4 != 1.0
void PS(
    out float4 outRT0 : SV_TARGET0, //albedo
    out float4 outRT1 : SV_TARGET1, //normal
    out float4 outRT2 : SV_TARGET2)
{ //emissive

    outRT0 = float4(0.0f, 0.0f, 0.0f, 0.0f);
    outRT1 = float4(0.5f, 0.5f, 0.0f, 0.0f);    //法线归一化到[0-1]，默认朝向没有偏移，归一化后正好是0.5
    outRT2 = float4(0.0f, 0.0f, 0.0f, 0.0f);
}