#include "D3DResource.h"

D3D12_RESOURCE_BARRIER InitResourceBarrier(
    ID3D12Resource* inResource, D3D12_RESOURCE_STATES inPrevState,
    D3D12_RESOURCE_STATES inNextState)
{
    D3D12_RESOURCE_BARRIER d3d12ResourceBarrier;
    memset(&d3d12ResourceBarrier, 0, sizeof(d3d12ResourceBarrier));
    d3d12ResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    d3d12ResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    d3d12ResourceBarrier.Transition.pResource = inResource;
    d3d12ResourceBarrier.Transition.StateBefore = inPrevState;
    d3d12ResourceBarrier.Transition.StateAfter = inNextState;
    d3d12ResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return d3d12ResourceBarrier;
}

void D3DResource::SetName(LPCTSTR inName)
{
    mName = inName;
    mUnderlyingResource->SetName(inName);
}

void D3DResource::SetSRV2DDesc(DXGI_FORMAT inFormat, int inMipLevels, int inMostDetailMip)
{
    mSRVDesc = {};
    mSRVDesc.Format = inFormat;
    mSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    mSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    mSRVDesc.Texture2D.MostDetailedMip = inMostDetailMip;
    mSRVDesc.Texture2D.MipLevels = inMipLevels;
}

void D3DResource::SetSRV2DArrayDesc(DXGI_FORMAT inFormat, UINT inArraySize, UINT inFirstSlice)
{
    mSRVDesc = {};
    mSRVDesc.Format = inFormat;
    mSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    mSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    mSRVDesc.Texture2DArray.FirstArraySlice = inFirstSlice;
    mSRVDesc.Texture2DArray.ArraySize = inArraySize;
    mSRVDesc.Texture2DArray.MipLevels = 1;
    mSRVDesc.Texture2DArray.MostDetailedMip = 0;
    mSRVDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
}

void D3DResource::SetSRV3DDesc(DXGI_FORMAT inFormat)
{
    mSRVDesc = {};
    mSRVDesc.Format = inFormat;
    mSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    mSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    mSRVDesc.Texture2D.MipLevels = 1;
}

void D3DResource::SetRW2DDesc(DXGI_FORMAT inFormat, int inMipSlice)
{
    mUAVDesc = {};
    mUAVDesc.Format = inFormat;
    mUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    mUAVDesc.Texture2D.MipSlice = inMipSlice;
}

void D3DResource::SetRW2DArrayDesc(DXGI_FORMAT inFormat, UINT inArraySize, UINT inFirstSlice)
{
    mUAVDesc = {};
    mUAVDesc.Format = inFormat;
    mUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    mUAVDesc.Texture2DArray.ArraySize = inArraySize;
    mUAVDesc.Texture2DArray.FirstArraySlice = inFirstSlice;
}

void D3DResource::SetRW3DDesc(DXGI_FORMAT inFormat)
{
    mUAVDesc = {};
    mUAVDesc.Format = inFormat;
    mUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
    mUAVDesc.Texture3D.FirstWSlice = 0;
    mUAVDesc.Texture3D.MipSlice = 0;
    mUAVDesc.Texture3D.WSize = -1;
}
