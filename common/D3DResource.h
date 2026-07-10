
#pragma once

#include "d3dx12.h"

#include <string>

struct D3DResource
{
    std::wstring mName;
    ID3D12Resource* mUnderlyingResource;
    D3D12_RESOURCE_STATES mState;
    D3D12_CONSTANT_BUFFER_VIEW_DESC mCBDesc;
    D3D12_SHADER_RESOURCE_VIEW_DESC mSRVDesc;
    D3D12_UNORDERED_ACCESS_VIEW_DESC mUAVDesc;
    D3DResource(D3D12_RESOURCE_STATES inState = D3D12_RESOURCE_STATE_COMMON)
        :mUnderlyingResource(nullptr), mState(inState), mCBDesc({}), mSRVDesc({}), mUAVDesc({}) {
    }
    void SetName(LPCTSTR inName);
    void SetSRV2DDesc(DXGI_FORMAT inFormat, int inMipLevels = 1, int inMostDetailMip = 0);
    void SetSRV2DArrayDesc(DXGI_FORMAT inFormat, UINT inArraySize, UINT inFirstSlice = 0);
    void SetSRV3DDesc(DXGI_FORMAT inFormat);

    void SetRW2DDesc(DXGI_FORMAT inFormat, int inMipSlice = 0);
    void SetRW2DArrayDesc(DXGI_FORMAT inFormat, UINT inArraySize, UINT inFirstSlice = 0);
    void SetRW3DDesc(DXGI_FORMAT inFormat);
    //void SetupUAV(D3D12Context& inContext);
    //void SetupSRV(D3D12Context& inContext);
};

D3D12_RESOURCE_BARRIER InitResourceBarrier(ID3D12Resource* inResource, D3D12_RESOURCE_STATES inPrevState, D3D12_RESOURCE_STATES inNextState);
