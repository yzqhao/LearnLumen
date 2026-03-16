
#include "d3dUtil.h"
#include <comdef.h>
#include <fstream>

using Microsoft::WRL::ComPtr;

DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
    ErrorCode(hr),
    FunctionName(functionName),
    Filename(filename),
    LineNumber(lineNumber)
{
}

bool d3dUtil::IsKeyDown(int vkeyCode)
{
    return (GetAsyncKeyState(vkeyCode) & 0x8000) != 0;
}

unsigned char* d3dUtil::LoadFileContent(const char* inFilePath, size_t& outFileSize) {
    FILE* file = nullptr;
    errno_t err = fopen_s(&file, inFilePath, "rb");
    if (err != 0) {
        return nullptr;
    }
    fseek(file, 0, SEEK_END);
    outFileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char* fileContent = new unsigned char[outFileSize];
    fread(fileContent, 1, outFileSize, file);
    fclose(file);
    return fileContent;
}

ComPtr<ID3DBlob> d3dUtil::LoadBinary(const std::wstring& filename)
{
    std::ifstream fin(filename, std::ios::binary);

    fin.seekg(0, std::ios_base::end);
    std::ifstream::pos_type size = (int)fin.tellg();
    fin.seekg(0, std::ios_base::beg);

    ComPtr<ID3DBlob> blob;
    ThrowIfFailed(D3DCreateBlob(size, blob.GetAddressOf()));

    fin.read((char*)blob->GetBufferPointer(), size);
    fin.close();

    return blob;
}

ID3D12Resource* d3dUtil::CreateDefaultBuffer(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const void* initData,
    UINT64 byteSize,
    ID3D12Resource*& uploadBuffer)
{
    ID3D12Resource* defaultBuffer;

    // Create the actual default buffer resource.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&defaultBuffer)));

    // In order to copy CPU memory data into our default buffer, we need to create
    // an intermediate upload heap. 
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)));


    // Describe the data we want to copy into the default buffer.
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    // Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
    // will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
    // the intermediate upload heap data will be copied to mBuffer.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer, 
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources<1>(cmdList, defaultBuffer, uploadBuffer, 0, 0, 1, &subResourceData);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer,
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

    // Note: uploadBuffer has to be kept alive after the above function calls because
    // the command list has not been executed yet that performs the actual copy.
    // The caller can Release the uploadBuffer after it knows the copy has been executed.


    return defaultBuffer;
}


static std::string wstring2string(const std::wstring& ws)
{
	_bstr_t t = ws.c_str();
	char* pchar = (char*)t;
	std::string result = pchar;
	return result;
}

IDxcBlob* d3dUtil::DxcCompileShader(
    const std::wstring& filename,
    const DxcDefine* defines,
    UINT defnums,
    const std::wstring& entrypoint,
    const std::wstring& target)
{
	IDxcLibrary* pLibrary;
	IDxcCompiler* pCompiler;
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler)));
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&pLibrary)));

	std::string readTexts;
	{
		FILE* file = NULL;
		fopen_s(&file, wstring2string(filename).c_str(), "rb");
		if (file == NULL)
			return NULL;

		fseek(file, 0, SEEK_END);
		int rawLength = ftell(file);
		fseek(file, 0, SEEK_SET);
		if (rawLength < 0)
		{
			fclose(file);
			return NULL;
		}

		readTexts.resize(rawLength);
		int readLength = fread(&*readTexts.begin(), 1, rawLength, file);
		fclose(file);
	}

	/************************************************************************/
	// Create blob from the string
	/************************************************************************/
	IDxcBlobEncoding* pTextBlob;
	ThrowIfFailed(pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)readTexts.c_str(), (UINT32)readTexts.size(), 0, &pTextBlob));
	IDxcOperationResult* pResult;

	IDxcIncludeHandler* pInclude = NULL;
	pLibrary->CreateIncludeHandler(&pInclude);

	/************************************************************************/
	// Compiler args
	/************************************************************************/

	const wchar_t* compileArgs[64];
	uint32_t numCompileArgs = 0;

	compileArgs[numCompileArgs++] = L"-Zi";
    compileArgs[numCompileArgs++] = L"-all_resources_bound";
    compileArgs[numCompileArgs++] = L"-Qembed_debug";
#if defined(DEBUG)
	compileArgs[numCompileArgs++] = L"-Od";
#else
	compileArgs[numCompileArgs++] = L"-O3";
#endif

	// specify the parent directory as include path
	//wchar_t directory[FS_MAX_PATH + 2] = L"-I";
	//mbstowcs(directory + 2, directoryStr, strlen(directoryStr));
	//ASSERT(numCompileArgs < MAX_COMPILE_ARGS);
	//compileArgs[numCompileArgs++] = directory;

	ThrowIfFailed(pCompiler->Compile(
		pTextBlob, filename.c_str(), entrypoint.c_str(), target.c_str(), compileArgs, numCompileArgs, defines, defnums, pInclude,
		&pResult));

	pInclude->Release();
	pLibrary->Release();
	pCompiler->Release();
	/************************************************************************/
	// Verify the result
	/************************************************************************/
	HRESULT resultCode;
	ThrowIfFailed(pResult->GetStatus(&resultCode));
	if (FAILED(resultCode))
	{
		IDxcBlobEncoding* pError;
        ThrowIfFailed(pResult->GetErrorBuffer(&pError));
        OutputDebugStringA((const char*)pError->GetBufferPointer());
		pError->Release();
		return NULL;
	}
	/************************************************************************/
	// Collect blob
	/************************************************************************/
	IDxcBlob* pBlob;
	ThrowIfFailed(pResult->GetResult(&pBlob));

	return pBlob;
}

ID3DBlob* d3dUtil::CompileShader(
	const std::wstring& filename,
	const D3D_SHADER_MACRO* defines,
	const std::string& entrypoint,
	const std::string& target)
{
	UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = S_OK;

	ID3DBlob* byteCode = nullptr;
	ID3DBlob* errors = nullptr;
	hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

	if(errors != nullptr)
		OutputDebugStringA((char*)errors->GetBufferPointer());

	ThrowIfFailed(hr);

	return byteCode;
}

std::wstring DxException::ToString()const
{
    // Get the string description of the error code.
    _com_error err(ErrorCode);
    std::wstring msg = err.ErrorMessage();

    return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
}


