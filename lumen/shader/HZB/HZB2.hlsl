
cbuffer _RootShaderParameters
{
    float4 DispatchThreadIdToBufferUV : packoffset(c0);
    uint4 PixelViewPortMinMax : packoffset(c1);
    float2 InputViewportMaxBound : packoffset(c2);
    float2 InvSize : packoffset(c2.z);
}

float min3(float a, float b, float c)
{
	return min(a, min(b, c));
}
float max3(float a, float b, float c)
{
	return max(a, max(b, c));
}
uint SignedRightShift(uint x, const int bitshift)
{
	if (bitshift > 0)
	{
		return x << asuint(bitshift);
	}
	else if (bitshift < 0)
	{
		return x >> asuint(-bitshift);
	}
	return x;
}
uint2 InitialTilePixelPositionForReduction2x2(const uint TileSizeLog2, uint SharedArrayId)
{
	uint x = 0;
	uint y = 0;
	[unroll]
	for (uint i = 0; i < TileSizeLog2; i++)
	{
		const uint DestBitId = TileSizeLog2 - 1 - i;
		const uint DestBitMask = 1u << DestBitId;
		x |= DestBitMask & SignedRightShift(SharedArrayId, int(DestBitId) - int(i * 2 + 0));
		y |= DestBitMask & SignedRightShift(SharedArrayId, int(DestBitId) - int(i * 2 + 1));
	}
	return uint2(x, y);
}
uint2 InitialTilePixelPositionForReduction2x2(const uint TileSizeLog2, const uint ReduceCount, uint SharedArrayId)
{
	uint2 p = InitialTilePixelPositionForReduction2x2(ReduceCount, SharedArrayId);
	SharedArrayId = SharedArrayId >> (2 * ReduceCount);
	const uint RemainingSize = 1u << (TileSizeLog2 - ReduceCount);
	p.x |= ((SharedArrayId % RemainingSize) << ReduceCount);
	p.y |= ((SharedArrayId / RemainingSize) << ReduceCount);
	return p;
}
Texture2D		ParentTextureMip;
SamplerState	ParentTextureMipSampler;
float4 Gather4(Texture2D Texture, SamplerState TextureSampler, float2 BufferUV, uint Level)
{
	float2 UV = min(BufferUV + float2(-0.25f, -0.25f) * InvSize, InputViewportMaxBound - InvSize);
	return Texture.GatherRed(TextureSampler, UV, Level);
}
float4 Gather4(Texture2D Texture, SamplerState TextureSampler, float2 BufferUV)
{
	return Gather4(Texture, TextureSampler, BufferUV, 0);
}
RWTexture2D<float> ClosestHZBOutput_0;
RWTexture2D<float> ClosestHZBOutput_1;
RWTexture2D<float> ClosestHZBOutput_2;
RWTexture2D<float> ClosestHZBOutput_3;
groupshared uint SharedMinDeviceZ[8 * 8];
groupshared float SharedMaxDeviceZ[8 * 8];
float RoundUpF16(float DeviceZ)
{
	return f16tof32(f32tof16(DeviceZ) + 1);
}
void OutputMipLevel(uint MipLevel, uint2 OutputPixelPos, float FurthestDeviceZ, float ClosestDeviceZ)
{
	if (MipLevel == 1)
	{
		ClosestHZBOutput_1[OutputPixelPos] = RoundUpF16(ClosestDeviceZ);
	}
	else if (MipLevel == 2)
	{
		ClosestHZBOutput_2[OutputPixelPos] = RoundUpF16(ClosestDeviceZ);
	}
	else if (MipLevel == 3)
	{
		ClosestHZBOutput_3[OutputPixelPos] = RoundUpF16(ClosestDeviceZ);
	}		
}

[numthreads(8, 8, 1)]
void CS(
	uint2 GroupId : SV_GroupID,
	uint GroupThreadIndex : SV_GroupIndex)
{
		uint2 GroupThreadId = InitialTilePixelPositionForReduction2x2(4 - 1, GroupThreadIndex);
	uint2 GroupOffset = 8 * GroupId;
	uint2 DispatchThreadId = GroupOffset + GroupThreadId;
	float2 BufferUV = (DispatchThreadId + 0.5) * DispatchThreadIdToBufferUV.xy + DispatchThreadIdToBufferUV.zw;
	float4 DeviceZ = Gather4(ParentTextureMip, ParentTextureMipSampler, BufferUV);
	float MinDeviceZ = min(min3(DeviceZ.x, DeviceZ.y, DeviceZ.z), DeviceZ.w);
	float MaxDeviceZ = max(max3(DeviceZ.x, DeviceZ.y, DeviceZ.z), DeviceZ.w);
	uint2 OutputPixelPos = DispatchThreadId;
		ClosestHZBOutput_0[OutputPixelPos] = RoundUpF16(MaxDeviceZ);
	{
		SharedMinDeviceZ[GroupThreadIndex] = asuint(MinDeviceZ);
		SharedMaxDeviceZ[GroupThreadIndex] = MaxDeviceZ;
		const uint LaneCount = WaveGetLaneCount();
		[unroll]
		for (uint MipLevel = 1; MipLevel < 4; ++MipLevel)
		{
			const uint TileSize = uint(8) >> MipLevel;
			const uint ReduceBankSize = TileSize * TileSize;
			if ((ReduceBankSize << 2u) > LaneCount)
			{
				GroupMemoryBarrierWithGroupSync();
			}
			[branch]
			if (GroupThreadIndex < ReduceBankSize)
			{
				float4 ParentMinDeviceZ;
				float4 ParentMaxDeviceZ;
				ParentMinDeviceZ[0] = MinDeviceZ;
				ParentMaxDeviceZ[0] = MaxDeviceZ;
				[unroll]
				for (uint i = 1; i < 4; i++)
				{
					uint LDSIndex = GroupThreadIndex + i * ReduceBankSize;
					ParentMinDeviceZ[i] = asfloat(SharedMinDeviceZ[LDSIndex]);
					ParentMaxDeviceZ[i] = SharedMaxDeviceZ[LDSIndex];
				}
				MinDeviceZ = min(min3(ParentMinDeviceZ.x, ParentMinDeviceZ.y, ParentMinDeviceZ.z), ParentMinDeviceZ.w);
				MaxDeviceZ = max(max3(ParentMaxDeviceZ.x, ParentMaxDeviceZ.y, ParentMaxDeviceZ.z), ParentMaxDeviceZ.w);
				OutputPixelPos = OutputPixelPos >> 1;
				OutputMipLevel(MipLevel, OutputPixelPos, MinDeviceZ, MaxDeviceZ);
				SharedMinDeviceZ[GroupThreadIndex] = asuint(MinDeviceZ);
				SharedMaxDeviceZ[GroupThreadIndex] = MaxDeviceZ;
			}
		}
	}
}
