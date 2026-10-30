#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cstdint>
namespace kz::shell {
struct PreviewItem{std::wstring name;bool directory=false;};
inline bool safeLabel(const std::wstring& s){return !s.empty()&&s.size()<=1024&&std::none_of(s.begin(),s.end(),[](wchar_t c){return c<32;});}
inline std::vector<PreviewItem> preview(const std::filesystem::path& file){
 try{

  auto ext=file.extension().wstring();if(_wcsicmp(ext.c_str(),L".zip")!=0)return {};
  struct Handle{HANDLE h=INVALID_HANDLE_VALUE;~Handle(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}} handle;
  handle.h=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_RANDOM_ACCESS,nullptr);
  if(handle.h==INVALID_HANDLE_VALUE)return {};LARGE_INTEGER size{};if(!GetFileSizeEx(handle.h,&size)||size.QuadPart<22)return {};
  auto read=[&](uint64_t offset,void* data,DWORD length){LARGE_INTEGER pos{};pos.QuadPart=static_cast<LONGLONG>(offset);DWORD count=0;
    return offset<=static_cast<uint64_t>(size.QuadPart)&&length<=static_cast<uint64_t>(size.QuadPart)-offset&&SetFilePointerEx(handle.h,pos,nullptr,FILE_BEGIN)&&ReadFile(handle.h,data,length,&count,nullptr)&&count==length;};
  const auto tailSize=static_cast<DWORD>(std::min<LONGLONG>(65557,size.QuadPart));std::vector<unsigned char> tail(tailSize);
  if(!read(static_cast<uint64_t>(size.QuadPart)-tailSize,tail.data(),tailSize))return {};
  auto u16=[](const unsigned char* p){return unsigned(p[0])|(unsigned(p[1])<<8);};
  auto u32=[](const unsigned char* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);};
  int end=-1;for(int i=static_cast<int>(tailSize)-22;i>=0;--i)if(u32(tail.data()+i)==0x06054b50&&i+22+u16(tail.data()+i+20)==static_cast<int>(tailSize)){end=i;break;}
  if(end<0)return {};auto p=tail.data()+end;if(u16(p+4)||u16(p+6)||u16(p+8)!=u16(p+10)||u16(p+10)==65535)return {};
  uint64_t offset=u32(p+16),limit=offset+u32(p+12);if(limit>static_cast<uint64_t>(size.QuadPart)-tailSize+end)return {};
  std::vector<PreviewItem> result;
  for(unsigned i=0;i<std::min<unsigned>(16,u16(p+10));++i){unsigned char header[46];if(offset>limit||46>limit-offset||!read(offset,header,46)||u32(header)!=0x02014b50)return {};
   unsigned length=u16(header+28),extra=u16(header+30),comment=u16(header+32);if(!length||length>4096||46ULL+length+extra+comment>limit-offset)return {};
   std::string bytes(length,'\0');if(!read(offset+46,bytes.data(),length))return {};
   UINT code=(u16(header+8)&0x800)?CP_UTF8:CP_ACP;DWORD flags=code==CP_UTF8?MB_ERR_INVALID_CHARS:0;
   int chars=MultiByteToWideChar(code,flags,bytes.data(),length,nullptr,0);if(chars<=0||chars>1024)return {};
   PreviewItem item;item.name.resize(chars);MultiByteToWideChar(code,flags,bytes.data(),length,item.name.data(),chars);
   if(!safeLabel(item.name))return {};item.directory=item.name.back()==L'/'||item.name.back()==L'\\';result.push_back(std::move(item));offset+=46ULL+length+extra+comment;
  }return result;
 }catch(...){return {};}
}
}
