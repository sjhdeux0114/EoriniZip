#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <fstream>
#include <algorithm>
#include "core.hpp"
#include "resource.h"
#include "ui_language.hpp"
namespace kz::win {
inline bool imagePreview(const fs::path& file){auto ext=file.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);return ext==L".png"||ext==L".jpg"||ext==L".jpeg"||ext==L".bmp"||ext==L".gif";}
inline bool textPreview(const fs::path& file){auto ext=file.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);for(auto value:{L".txt",L".md",L".json",L".xml",L".csv",L".log",L".ini",L".yaml",L".yml",L".cpp",L".h",L".c",L".cs",L".py"})if(ext==value)return true;return false;}
struct PreviewData{fs::path file;HBITMAP bitmap=nullptr;std::wstring contents,error;};
inline INT_PTR CALLBACK previewProcedure(HWND dialog,UINT message,WPARAM wp,LPARAM lp){
 if(message==WM_INITDIALOG){
  kz::ui::dialog(dialog);
  auto data=reinterpret_cast<PreviewData*>(lp);SetWindowLongPtrW(dialog,DWLP_USER,lp);SetWindowTextW(dialog,(data->file.filename().wstring()+(kz::ui::english?L" - Preview - EoriniZip":L" - 미리보기 - 어린이zip")).c_str());
  try{
   if(imagePreview(data->file)){
    ShowWindow(GetDlgItem(dialog,IDC_PREVIEW_TEXT),SW_HIDE);
    Gdiplus::Bitmap original(data->file.c_str());auto width=original.GetWidth(),height=original.GetHeight();
    if(original.GetLastStatus()!=Gdiplus::Ok||!width||!height||uint64_t(width)*height>32000000)throw std::runtime_error("이미지를 읽을 수 없거나 미리보기 크기 한도를 초과했습니다.");
    RECT bounds{};GetClientRect(GetDlgItem(dialog,IDC_PREVIEW_IMAGE),&bounds);double scale=std::min(1.0,std::min(double(bounds.right)/width,double(bounds.bottom)/height));
    UINT w=std::max(1u,static_cast<UINT>(width*scale)),h=std::max(1u,static_cast<UINT>(height*scale));
    Gdiplus::Bitmap scaled(w,h,PixelFormat32bppARGB);Gdiplus::Graphics graphics(&scaled);graphics.Clear(Gdiplus::Color(255,255,255));graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);graphics.DrawImage(&original,0,0,w,h);
    if(scaled.GetHBITMAP(Gdiplus::Color(255,255,255),&data->bitmap)!=Gdiplus::Ok)throw std::runtime_error("이미지 미리보기 생성 실패");
    SendDlgItemMessageW(dialog,IDC_PREVIEW_IMAGE,STM_SETIMAGE,IMAGE_BITMAP,reinterpret_cast<LPARAM>(data->bitmap));
   }else{
    ShowWindow(GetDlgItem(dialog,IDC_PREVIEW_IMAGE),SW_HIDE);if(fs::file_size(data->file)>2*1024*1024)throw std::runtime_error("텍스트 미리보기는 2 MiB까지 지원합니다.");
    std::ifstream input(data->file,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(input)),{});
    if(!input&&!input.eof())throw std::runtime_error("텍스트를 읽을 수 없습니다.");
    if(bytes.size()>=2&&static_cast<unsigned char>(bytes[0])==255&&static_cast<unsigned char>(bytes[1])==254){for(size_t i=2;i+1<bytes.size();i+=2)data->contents.push_back(static_cast<wchar_t>(static_cast<unsigned char>(bytes[i])|(static_cast<unsigned char>(bytes[i+1])<<8)));}
    else{
     if(bytes.compare(0,3,"\xef\xbb\xbf")==0)bytes.erase(0,3);
     int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),static_cast<int>(bytes.size()),nullptr,0);UINT encoding=CP_UTF8;DWORD flags=MB_ERR_INVALID_CHARS;
     if(!count&&!bytes.empty()){encoding=CP_ACP;flags=0;count=MultiByteToWideChar(encoding,flags,bytes.data(),static_cast<int>(bytes.size()),nullptr,0);}
     data->contents.resize(count);if(count)MultiByteToWideChar(encoding,flags,bytes.data(),static_cast<int>(bytes.size()),data->contents.data(),count);
    }
    for(auto& c:data->contents)if(c==L'\0')c=L'\xfffd';SetDlgItemTextW(dialog,IDC_PREVIEW_TEXT,data->contents.c_str());
   }
  }catch(const std::exception& e){ShowWindow(GetDlgItem(dialog,IDC_PREVIEW_IMAGE),SW_HIDE);ShowWindow(GetDlgItem(dialog,IDC_PREVIEW_TEXT),SW_SHOW);data->error=widen(e.what());SetDlgItemTextW(dialog,IDC_PREVIEW_TEXT,data->error.c_str());}
  return TRUE;
 }
 if(message==WM_CLOSE||(message==WM_COMMAND&&(LOWORD(wp)==IDOK||LOWORD(wp)==IDCANCEL))){EndDialog(dialog,IDOK);return TRUE;}return FALSE;
}
inline void showPreview(HWND owner,const fs::path& file){PreviewData data;data.file=file;DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_PREVIEW),owner,previewProcedure,reinterpret_cast<LPARAM>(&data));if(data.bitmap)DeleteObject(data.bitmap);}
}
