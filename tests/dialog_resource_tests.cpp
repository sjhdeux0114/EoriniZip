#include <windows.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "resource.h"
struct Item {DWORD style;DWORD id;};
static std::vector<Item> items(HMODULE module,int resource){
 HRSRC found=FindResourceW(module,MAKEINTRESOURCEW(resource),RT_DIALOG);if(!found)throw std::runtime_error("Missing dialog resource");
 auto start=static_cast<const BYTE*>(LockResource(LoadResource(module,found)));auto end=start+SizeofResource(module,found);auto p=start;
 auto word=[&](){if(p+2>end)throw std::runtime_error("Truncated resource");WORD value;memcpy(&value,p,2);p+=2;return value;};
 auto dword=[&](){DWORD low=word(),high=word();return low|(high<<16);};
 auto skip=[&](){auto value=word();if(value==0xffff){word();return;}while(value)value=word();};
 if(word()!=1||word()!=0xffff)throw std::runtime_error("Expected extended dialog");
 dword();dword();DWORD style=dword();auto count=word();for(int i=0;i<4;++i)word();skip();skip();skip();
 if(style&DS_SETFONT){word();word();word();skip();}
 std::vector<Item> result;
 for(unsigned i=0;i<count;++i){p=start+((p-start+3)&~3);dword();dword();auto itemStyle=dword();for(int k=0;k<4;++k)word();auto id=dword();skip();skip();auto extra=word();p+=extra;if(p>end)throw std::runtime_error("Invalid item size");result.push_back({itemStyle,id});}
 return result;
}
int wmain(int argc,wchar_t** argv){try{
 if(argc!=2)throw std::runtime_error("Supply executable");auto module=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);if(!module)throw std::runtime_error("Cannot read application resources");
 auto password=items(module,IDD_DECRYPT);bool masked=false,ok=false,cancel=false;
 for(auto item:password){if(item.id==IDC_PASSWORD)masked=(item.style&ES_PASSWORD)&&(item.style&WS_TABSTOP);ok|=item.id==IDOK;cancel|=item.id==IDCANCEL;}
 if(!masked||!ok||!cancel)throw std::runtime_error("Password dialog must mask input and expose confirm/cancel");
 auto settings=items(module,IDD_SETTINGS);bool all=false,none=false;for(auto item:settings){all|=item.id==IDC_ASSOC_ALL;none|=item.id==IDC_ASSOC_NONE;}
 if(!all||!none)throw std::runtime_error("Missing association selection buttons");
 FreeLibrary(module);std::cout<<"Password dialog mask/confirm/cancel and association selection resources passed (not a UI interaction test)\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}