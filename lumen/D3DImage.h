#pragma once
#include "../common/d3dUtil.h"

inline D3D12_RESOURCE_BARRIER InitResourceBarrier(
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

struct D3DImage :public D3DResource
{
    bool mbIsRT;
    D3D12_CLEAR_VALUE mClearValue;
    DXGI_FORMAT mFormat;
    DXGI_FORMAT mSRVFormat;
    D3D12_RESOURCE_DIMENSION mResourceDimension;
    union {
        DXGI_FORMAT mRTVFormat;
        DXGI_FORMAT mDSVFormat;
    };
    D3DImage(bool inIsRT = false) :mbIsRT(inIsRT), D3DResource(D3D12_RESOURCE_STATE_GENERIC_READ) {
        mClearValue = {};
    }
    D3DImage* FileImageToUAVImage(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* cmdList);
    static D3DImage* InitTextureFromFile(LPCTSTR inImagePath);
    static D3DImage* InitD32S8FromFile(LPCTSTR inImagePath);
};

D3DImage* Init2DRTImage(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* cmdList, UINT64 inWidth, UINT64 inHeight, UINT64 inAlignment, DXGI_FORMAT inFormat, DXGI_FORMAT inSRVFormat,
    DXGI_FORMAT inRTFormat, D3D12_RESOURCE_FLAGS inFlags, D3D12_CLEAR_VALUE inClearValue, int inMipLevelCount = 1);

D3DImage* Init2DRTImage3(ID3D12Device10* d3dDevice, UINT64 inWidth, UINT64 inHeight,
    DXGI_FORMAT inFormat, DXGI_FORMAT inSRVFormat, DXGI_FORMAT inRTFormat,
    D3D12_RESOURCE_FLAGS inFlags, DXGI_FORMAT* inCastableFormats, int inCastableFormatCount);
