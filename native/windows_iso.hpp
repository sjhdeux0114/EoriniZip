#pragma once
#include <windows.h>
#include <imapi2fs.h>
#include <wrl/client.h>
#include <fstream>
#include <vector>
#include "core.hpp"
namespace kz {
inline void writeIsoImage(const fs::path& folder,const fs::path& output,const std::atomic<bool>& cancelled,std::atomic<int>& progress){
  struct Apartment{HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);~Apartment(){if(SUCCEEDED(result))CoUninitialize();}} apartment;
  if(FAILED(apartment.result)&&apartment.result!=RPC_E_CHANGED_MODE)throw std::runtime_error("Cannot initialize ISO image builder.");
  auto check=[](HRESULT result){if(FAILED(result))throw std::runtime_error("Windows ISO image creation failed (HRESULT "+std::to_string(static_cast<unsigned long>(result))+").");};
  struct Text{BSTR value;explicit Text(const std::wstring& text):value(SysAllocStringLen(text.data(),static_cast<UINT>(text.size()))){if(!value)throw std::bad_alloc();}~Text(){SysFreeString(value);}} source(fs::absolute(folder).wstring()),label(L"EORINIZIP");
  Microsoft::WRL::ComPtr<IFileSystemImage> image;
  check(CoCreateInstance(__uuidof(MsftFileSystemImage),nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&image)));
  check(image->put_FreeMediaBlocks(11000000)); // Enough for the core's 20 GiB input limit plus metadata.
  check(image->put_FileSystemsToCreate(static_cast<FsiFileSystems>(FsiFileSystemISO9660|FsiFileSystemJoliet|FsiFileSystemUDF)));
  check(image->put_VolumeName(label.value));
  Microsoft::WRL::ComPtr<IFsiDirectoryItem> root;check(image->get_Root(&root));check(root->AddTree(source.value,VARIANT_FALSE));
  if(cancelled)throw std::runtime_error("Operation cancelled.");
  Microsoft::WRL::ComPtr<IFileSystemImageResult> result;check(image->CreateResultImage(&result));
  Microsoft::WRL::ComPtr<IStream> stream;check(result->get_ImageStream(&stream));STATSTG info{};check(stream->Stat(&info,STATFLAG_NONAME));
  std::ofstream file(output,std::ios::binary);if(!file)throw std::runtime_error("Cannot create ISO output.");
  std::vector<char> buffer(1024*1024);uint64_t total=0;
  for(;;){
    if(cancelled)throw std::runtime_error("Operation cancelled.");
    ULONG read=0;HRESULT status=stream->Read(buffer.data(),static_cast<ULONG>(buffer.size()),&read);check(status);
    if(!read)break;file.write(buffer.data(),read);if(!file)throw std::runtime_error("Cannot write ISO image.");
    total+=read;if(info.cbSize.QuadPart)progress=static_cast<int>((total*100)/info.cbSize.QuadPart);
    if(status==S_FALSE)break;
  }
  file.close();if(!file||total!=info.cbSize.QuadPart)throw std::runtime_error("Incomplete ISO image.");
}
}
