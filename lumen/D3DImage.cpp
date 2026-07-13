#include "D3DImage.h"

D3DImage* D3DImage::InitTextureFromFile(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* cmdList, LPCTSTR inImagePath)
{
    ID3D12Resource* texture = nullptr;
    ID3D12Resource* textureUploadHeap = nullptr;
    HRESULT hr = DirectX::CreateDDSTextureFromFile12(
        d3dDevice,
        cmdList,
        inImagePath,
        texture,
        textureUploadHeap
    );
    if (FAILED(hr)) {
        wprintf(L"InitTextureFromFile [%s] failed\n", inImagePath);
        return nullptr;
    }
    D3D12_RESOURCE_DESC resourceDesc = texture->GetDesc();
    D3DImage* image = new D3DImage;
    image->mUnderlyingResource = texture;
    image->mFormat = resourceDesc.Format;
    image->mSRVFormat = resourceDesc.Format;
    image->mRTVFormat = resourceDesc.Format;
    image->mClearValue = {};
    return image;
}

D3DImage* Init2DRTImage(ID3D12Device* d3dDevice, ID3D12GraphicsCommandList* cmdList, UINT64 inWidth, UINT64 inHeight, UINT64 inAlignment, DXGI_FORMAT inFormat, DXGI_FORMAT inSRVFormat, DXGI_FORMAT inRTFormat, D3D12_RESOURCE_FLAGS inFlags, D3D12_CLEAR_VALUE inClearValue, int inMipLevelCount /*= 1*/)
{
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = inAlignment;
    resourceDesc.Width = inWidth;
    resourceDesc.Height = inHeight;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = inMipLevelCount;
    resourceDesc.Format = inFormat;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = inFlags;

    D3D12_HEAP_PROPERTIES d3dHeapProperties = {};
    d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//gpu
    ID3D12Resource* resource;
    HRESULT hResult = d3dDevice->CreateCommittedResource(
        &d3dHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        ((inFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)||(inFlags& D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) ? &inClearValue : nullptr,
        IID_PPV_ARGS(&resource)
    );
    if (hResult != S_OK) {
        throw std::runtime_error("Init2DRTImage Failed");
    }
    D3DImage* image = new D3DImage(true);
    image->mUnderlyingResource = resource;
    image->mFormat = inFormat;
    image->mSRVFormat = inSRVFormat;
    image->mRTVFormat = inRTFormat;
    image->mClearValue = inClearValue;
    image->mResourceDimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    return image;
}

D3DImage* Init2DRTImage3(ID3D12Device10* d3dDevice, ID3D12GraphicsCommandList* cmdList, UINT64 inWidth, UINT64 inHeight,
    DXGI_FORMAT inFormat, DXGI_FORMAT inSRVFormat, DXGI_FORMAT inRTFormat,
    D3D12_RESOURCE_FLAGS inFlags, DXGI_FORMAT* inCastableFormats, int inCastableFormatCount) {
    D3D12_RESOURCE_DESC1 resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0u;
    resourceDesc.Width = inWidth;
    resourceDesc.Height = inHeight;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = inFormat;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = inFlags;

    D3D12_HEAP_PROPERTIES d3dHeapProperties = {};
    d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//gpu
    ID3D12Resource* resource;
    HRESULT hResult = d3dDevice->CreateCommittedResource3(
        &d3dHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_BARRIER_LAYOUT_COMMON,
        nullptr,
        nullptr,
        inCastableFormatCount,
        inCastableFormats,
        IID_PPV_ARGS(&resource)
    );
    if (hResult != S_OK) {
        throw std::runtime_error("Init2DRTImage Failed");
    }
    // 使用传统屏障将资源从 COMMON 状态转换出来
    D3D12_RESOURCE_BARRIER barrier = InitResourceBarrier(resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ);
    cmdList->ResourceBarrier(1, &barrier);

    D3DImage* image = new D3DImage(true);
    image->mUnderlyingResource = resource;
    image->mFormat = inFormat;
    image->mSRVFormat = inSRVFormat;
    image->mRTVFormat = inRTFormat;
    image->mClearValue = {};
    image->mResourceDimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    return image;
}