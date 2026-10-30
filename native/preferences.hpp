#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <stdexcept>
#include <charconv>
namespace kz {
struct Preferences {
  int escape=1,grid=0,fullRow=1,startHome=1,test=0;
  int unicodeNames=1,storeTimes=1,excludeThumbs=0,parallel=1,zone=1,codePageIndex=0;
  int showTree=1,showStatus=1,theme=0x15b3f0,level=2;
  int closeAfterCreate=0,closeAfterExtract=0;
};
struct PreferenceField {const char* name;int Preferences::*member;int maximum;};
inline const std::vector<PreferenceField>& preferenceFields(){
  static const std::vector<PreferenceField> fields={
    {"Escape",&Preferences::escape,1},{"Grid",&Preferences::grid,1},{"FullRow",&Preferences::fullRow,1},
    {"StartHome",&Preferences::startHome,1},{"TestAfter",&Preferences::test,1},{"Level",&Preferences::level,3},
    {"ShowTree",&Preferences::showTree,1},{"ShowStatus",&Preferences::showStatus,1},{"Theme",&Preferences::theme,0xffffff},
    {"UnicodeNames",&Preferences::unicodeNames,1},{"StoreTimes",&Preferences::storeTimes,1},{"ExcludeThumbs",&Preferences::excludeThumbs,1},
    {"Parallel",&Preferences::parallel,1},{"PropagateZone",&Preferences::zone,1},{"CodePageIndex",&Preferences::codePageIndex,6},
    {"CloseAfterCreate",&Preferences::closeAfterCreate,1},{"CloseAfterExtract",&Preferences::closeAfterExtract,1}
  };return fields;
}
inline std::string exportPreferences(const Preferences& p){
  std::ostringstream out;out<<"[EoriniZip]\nVersion=1\n";
  for(const auto& f:preferenceFields()){
    int value=p.*(f.member);if(value<0||value>f.maximum)throw std::runtime_error("설정 값이 범위를 벗어났습니다.");
    out<<f.name<<"="<<value<<"\n";
  }return out.str();
}
inline Preferences importPreferences(const std::string& input){
  if(input.size()>65536)throw std::runtime_error("설정 파일이 너무 큽니다.");
  Preferences p;std::istringstream in(input);std::string line;std::set<std::string> seen;bool section=false,version=false;
  auto trim=[](std::string s){auto first=s.find_first_not_of(" \t\r");if(first==std::string::npos)return std::string{};return s.substr(first,s.find_last_not_of(" \t\r")-first+1);};
  while(std::getline(in,line)){
    line=trim(line);if(line.empty()||line[0]==';'||line[0]=='#')continue;
    if(line=="[EoriniZip]"){if(section)throw std::runtime_error("중복된 설정 구역입니다.");section=true;continue;}
    if(!section)throw std::runtime_error("어린이zip 설정 파일이 아닙니다.");
    auto pos=line.find('=');if(pos==std::string::npos)throw std::runtime_error("설정 형식이 잘못되었습니다.");
    auto name=trim(line.substr(0,pos)),text=trim(line.substr(pos+1));
    if(!seen.insert(name).second)throw std::runtime_error("중복된 설정 항목입니다.");
    int value=0;auto converted=std::from_chars(text.data(),text.data()+text.size(),value);
    if(converted.ec!=std::errc{}||converted.ptr!=text.data()+text.size())throw std::runtime_error("설정 값이 숫자가 아닙니다.");
    if(name=="Version"){if(value!=1)throw std::runtime_error("지원하지 않는 설정 버전입니다.");version=true;continue;}
    bool found=false;
    for(const auto& f:preferenceFields())if(name==f.name){if(value<0||value>f.maximum)throw std::runtime_error("설정 값이 범위를 벗어났습니다.");p.*(f.member)=value;found=true;break;}
    if(!found)throw std::runtime_error("알 수 없는 설정 항목입니다: "+name);
  }
  if(!section||!version)throw std::runtime_error("설정 버전 정보가 없습니다.");
  return p;
}
}
