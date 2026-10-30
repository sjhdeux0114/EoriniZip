#include "core.hpp"
#include "archive_name.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <random>
#include <regex>
#include <fstream>
#include <codecvt>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <stdio.h>
#include <mach-o/dyld.h>
#endif
namespace kz {
static std::string archiveExtension(const fs::path& path) {
  auto ext=path.extension().u8string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return ext;
}
fs::path Engine::codec(const std::string& name) const {
#ifdef _WIN32
  wchar_t buffer[32768];auto length=GetModuleFileNameW(nullptr,buffer,32768);
  if(!length||length>=32768)throw std::runtime_error("Cannot locate application directory.");
  auto directory=fs::path(buffer).parent_path();
  auto path=directory/fs::u8path("eorinizip-"+name+".exe");
#elif defined(__APPLE__)
  char buffer[4096];uint32_t size=sizeof(buffer);
  if(_NSGetExecutablePath(buffer,&size))throw std::runtime_error("Cannot locate application directory.");
  auto path=fs::weakly_canonical(buffer).parent_path()/fs::u8path("eorinizip-"+name);
#else
  auto path=executable_.parent_path()/fs::u8path("eorinizip-"+name);
#endif
  if(!fs::is_regular_file(path))throw std::runtime_error("Missing archive engine: "+path.filename().u8string());
  return path;
}std::wstring widen(const std::string& s) { return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(s); }
std::string narrow(const std::wstring& s) { return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(s); }
std::wstring quoteWindows(const std::wstring& text) {
  std::wstring out = L"\""; size_t slashes = 0;
  for (wchar_t c : text) {
    if (c == L'\\') { ++slashes; continue; }
    out.append(slashes * (c == L'"' ? 2 : 1), L'\\'); slashes = 0;
    if (c == L'"') out += L'\\';
    out += c;
  }
  out.append(slashes * 2, L'\\'); return out + L'"';
}
static void passwordCheck(const std::string& p) {
  if (p.size() > 4096 || p.find_first_of(std::string("\0\r\n", 3)) != std::string::npos) throw std::runtime_error("Invalid password.");
}
std::vector<std::string> createArgs(const Options& o, const fs::path& output, const std::vector<fs::path>& inputs) {
  if(o.format=="zpaq"){
    if(inputs.empty()||o.level<0||o.level>9)throw std::runtime_error("Choose files and a valid compression level.");
    passwordCheck(o.password);
    if(!o.split.empty()||o.hideNames)throw std::runtime_error("ZPAQ split and separate filename-encryption options are not supported.");
    std::vector<std::string> args={"a",output.u8string()};
    for(const auto& input:inputs)args.push_back(fs::absolute(input).u8string());
    args.push_back("-to");
    std::set<std::string> names;
    for(const auto& input:inputs){auto name=fs::absolute(input).filename().u8string();auto key=name;std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
      if(name.empty()||!names.insert(key).second)throw std::runtime_error("Input names collide.");
      args.push_back(name);
    }
    args.insert(args.end(),{"-method",std::to_string(o.level==0?0:o.level<=1?1:o.level<=5?3:5),"-threads",o.multiThread?"0":"1"});
    if(!o.password.empty())args.insert(args.end(),{"-key",o.password});
    if(o.excludeThumbs)args.insert(args.end(),{"-not","*/Thumbs.db","Thumbs.db"});
    return args;
  }  if (o.format != "zip" && o.format != "7z" && o.format != "tar") throw std::runtime_error("Unsupported creation format.");
  if (o.level < 0 || o.level > 9 || inputs.empty()) throw std::runtime_error("Choose input files and a valid compression level.");
  passwordCheck(o.password);
  if (!o.split.empty() && !std::regex_match(o.split, std::regex("[1-9][0-9]{0,12}[bBkKmMgG]"))) throw std::runtime_error("Split size must look like 100m or 2g.");
  if (o.format == "tar" && (!o.password.empty() || !o.split.empty() || o.hideNames)) throw std::runtime_error("TAR does not support encryption/split options.");
  if (o.hideNames && (o.format != "7z" || o.password.empty())) throw std::runtime_error("Filename encryption requires 7Z and a password.");
  std::vector<std::string> args = {"a", "-t" + o.format, "-mx=" + std::to_string(o.level), "-bsp1", "-y", "-sccUTF-8", "-spd"};
  if(o.format!="tar"){
    args.push_back(o.multiThread?"-mmt=on":"-mmt=off");
    args.push_back(o.storeTimes?"-mtc=on":"-mtc=off");args.push_back(o.storeTimes?"-mta=on":"-mta=off");
  }
  if(o.format=="zip")args.push_back(o.unicodeNames?"-mcu=on":"-mcu=off");
  if(o.excludeThumbs)args.push_back("-xr!Thumbs.db");
  if (!o.password.empty()) { args.push_back("-p" + o.password); if (o.format == "zip") args.push_back("-mem=AES256"); }
  if (o.hideNames) args.push_back("-mhe=on");
  if (!o.split.empty()) args.push_back("-v" + o.split);
  args.push_back("--"); args.push_back(output.u8string());
  for (const auto& input : inputs) args.push_back(fs::absolute(input).u8string());
  return args;
}
std::vector<Entry> parseListing(const std::string& text) {
  std::vector<Entry> entries; std::map<std::string,std::string> fields;
  auto flush = [&] {
    if (fields.count("Path")) {
      Entry e; e.name = fields["Path"]; e.modified = fields["Modified"];
      auto number = [&](const char* key) -> uint64_t {
        const auto& v = fields[key]; if (v.empty()) return 0;
        if (!std::all_of(v.begin(), v.end(), [](unsigned char c){ return std::isdigit(c); })) throw std::runtime_error("Invalid entry size.");
        return std::stoull(v);
      };
      e.size = number("Size"); e.packed = number("Packed Size");
      const auto& a = fields["Attributes"];
      e.directory = fields["Folder"] == "+" || (!a.empty() && a[0] == 'D');
      e.encrypted = fields["Encrypted"] == "+";
      e.link = !fields["Symbolic Link"].empty() || !fields["Hard Link"].empty() || std::regex_search(a + " " + fields["Mode"], std::regex("(^| )l[rwxstST-]{9}"));
      entries.push_back(e);
    } fields.clear();
  };
  std::istringstream stream(text); std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) { flush(); continue; }
    auto pos = line.find(" = "); if (pos != std::string::npos) fields[line.substr(0, pos)] = line.substr(pos + 3);
  }
  flush(); return entries;
}
void validateEntries(const std::vector<Entry>& entries) {
  if (entries.size() > 100000) throw std::runtime_error("Safety limit: more than 100,000 entries.");
  uint64_t total = 0; std::set<std::string> seen;
  for (const auto& e : entries) {
    std::string name = e.name; std::replace(name.begin(), name.end(), '\\', '/');
    if (name.empty() || name[0] == '/' || name.find(':') != std::string::npos || e.link) throw std::runtime_error("Unsafe path or link in archive.");
    for (unsigned char c : name) if (c < 32) throw std::runtime_error("Control character in path.");
    if (name.back() == '/') name.pop_back();
    std::istringstream parts(name); std::string part;
    while (std::getline(parts, part, '/')) {
      if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ' || std::regex_match(part, std::regex("(con|prn|aux|nul|com[0-9]|lpt[0-9])(\\..*)?", std::regex::icase))) throw std::runtime_error("Unsafe archive path.");
    }
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    if (!seen.insert(name).second) throw std::runtime_error("Duplicate archive path.");
    constexpr uint64_t limit = 20ULL * 1024 * 1024 * 1024;
    if (e.size > limit - total) throw std::runtime_error("Safety limit: expanded data exceeds 20 GiB.");
    total += e.size;
  }
}
static fs::path freshDirectory(const fs::path& parent) {
  std::random_device random;
  for (int i = 0; i < 32; ++i) {
    auto p = parent / (".eorinizip-tmp-" + std::to_string(random()) + "-" + std::to_string(random()));
    if (fs::create_directory(p)) {
#ifdef _WIN32
      SetFileAttributesW(p.c_str(),FILE_ATTRIBUTE_HIDDEN);
#endif
      return p;
    }
  }
  throw std::runtime_error("Cannot create output directory.");
}
std::vector<Entry> Engine::list(const fs::path& archive, const std::string& password) {
  auto ext = archive.extension().u8string(); std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){return static_cast<char>(std::tolower(c));});
  if (ext == ".ezc") throw std::runtime_error("도구 메뉴의 EasyCrypt 파일 해제를 사용하세요.");
  if(ext==".alz"||ext==".egg"||ext==".zpaq"){
    passwordCheck(password);
    std::vector<std::string> args;
    if(ext==".zpaq"){
      args={"--eorinizip","l",fs::absolute(archive).u8string()};
      if(!password.empty())args.insert(args.end(),{"-key",password});
    }else args={"--eorinizip-list","-q","--",fs::absolute(archive).u8string()};
    auto entries=parseListing(run(args,codec(ext==".alz"?"unalz":ext==".egg"?"unegg":"zpaq"),4));
    validateEntries(entries);return entries;
  }
  passwordCheck(password);
  std::vector<std::string> args = {"l", "-slt", "-ba", "-sccUTF-8"};
  if (!password.empty()) args.push_back("-p" + password);
  if(codePage_&&ext==".zip"&&std::find(args.begin(),args.end(),"-mcp="+std::to_string(codePage_))==args.end())args.push_back("-mcp="+std::to_string(codePage_));
  args.insert(args.end(), {"--", fs::absolute(archive).u8string()}); auto entries=parseListing(run(args));validateEntries(entries);return entries;
}
void Engine::test(const fs::path& archive, const std::string& password) {
  auto ext=archiveExtension(archive);
  if(ext==".alz"||ext==".egg"||ext==".zpaq"){
    auto temp=freshDirectory(fs::temp_directory_path());
    try{extract(archive,temp,password);fs::remove_all(temp);return;}
    catch(...){std::error_code ec;fs::remove_all(temp,ec);throw;}
  }  passwordCheck(password); std::vector<std::string> args = {"t", "-bsp1", "-sccUTF-8"};
  if (!password.empty()) args.push_back("-p" + password);
  args.insert(args.end(), {"--", fs::absolute(archive).u8string()}); run(args);
}
static void moveExclusive(const fs::path& from,const fs::path& to) {
#ifdef _WIN32
  if(!MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_COPY_ALLOWED))throw fs::filesystem_error("Move output",from,to,std::error_code(GetLastError(),std::system_category()));
#elif defined(__APPLE__)
  if(renamex_np(from.c_str(),to.c_str(),RENAME_EXCL)){
    if(errno!=EXDEV||!fs::is_regular_file(from))throw fs::filesystem_error("Move output",from,to,std::error_code(errno,std::generic_category()));
    fs::copy_file(from,to,fs::copy_options::none);fs::remove(from);
  }
#else
  if(fs::exists(to))throw std::runtime_error("Output already exists");
  fs::rename(from,to);
#endif
}
static fs::path numbered(const fs::path& desired,unsigned number) {
  if(!number)return desired;
  return desired.parent_path()/fs::u8path(desired.stem().u8string()+" ("+std::to_string(number)+")"+desired.extension().u8string());
}
void Engine::edit(const fs::path& input,const std::string& action,const std::vector<std::string>& names,const std::string& password) {
  auto archive=fs::absolute(input);auto ext=archiveExtension(archive);
  if(ext!=".zip"&&ext!=".7z")throw std::runtime_error("내부 편집은 단일 ZIP / 7Z 파일에서 지원합니다.");
  if(names.empty()||(action!="a"&&action!="d"&&action!="rn")||(action=="rn"&&names.size()!=2))throw std::runtime_error("Invalid archive edit request.");
  auto entries=list(archive,password);validateEntries(entries);
  bool encrypted=std::any_of(entries.begin(),entries.end(),[](const Entry& e){return e.encrypted;});
  if(encrypted&&password.empty())throw PasswordRequired("압축 파일을 수정하려면 암호를 입력하세요.");
  if(action!="a"){
    auto normalize=[](std::string value){std::replace(value.begin(),value.end(),'\\','/');while(!value.empty()&&value.back()=='/')value.pop_back();return value;};
    for(size_t i=0;i<(action=="rn"?1:names.size());++i){
      if(std::none_of(entries.begin(),entries.end(),[&](const Entry& e){return normalize(e.name)==normalize(names[i]);}))throw std::runtime_error("선택한 항목이 없습니다.");
    }
    if(action=="rn"){
      Entry target;target.name=names[1];validateEntries({target});
      auto old=normalize(names[0]),next=normalize(names[1]);
      if(old==next)return;
      for(auto& e:entries){auto name=normalize(e.name);if(name==old||name.rfind(old+"/",0)==0)e.name=next+name.substr(old.size());}
      validateEntries(entries);
    }
  }else{
    std::set<std::string> basenames;
    for(const auto& name:names){auto path=fs::u8path(name);if(!fs::is_regular_file(path)||fs::is_symlink(path))throw std::runtime_error("추가할 일반 파일을 선택하세요.");
      auto base=path.filename().u8string();Entry e;e.name=base;validateEntries({e});
      std::transform(base.begin(),base.end(),base.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
      if(!basenames.insert(base).second)throw std::runtime_error("추가할 파일의 이름이 서로 같습니다.");
    }
  }
  const auto size=fs::file_size(archive);const auto stamp=fs::last_write_time(archive);
  auto staging=freshDirectory(archive.parent_path());auto working=staging/archive.filename();
  try{
    fs::copy_file(archive,working);
    std::vector<std::string> args={action,"-y","-bsp1","-sccUTF-8","-scsUTF-8","-spd"};
    if(!password.empty())args.push_back("-p"+password);
    if(action=="a"&&ext==".zip"&&encrypted)args.push_back("-mem=AES256");
    if(action=="rn"){args.insert(args.end(),{"--",working.u8string(),names[0],names[1]});}
    else{
      auto listFile=staging/"edit-inputs.txt";std::ofstream listOut(listFile,std::ios::binary);
      for(const auto& name:names){if(name.find_first_of("\r\n")!=std::string::npos)throw std::runtime_error("Invalid filename.");listOut<<(action=="a"?fs::absolute(fs::u8path(name)).u8string():name)<<"\n";}
      listOut.close();if(!listOut)throw std::runtime_error("Cannot write edit selection.");
      args.push_back("-i@"+listFile.u8string());args.insert(args.end(),{"--",working.u8string()});
    }
    run(args,{},1);auto updated=list(working,password);validateEntries(updated);test(working,password);
    if(cancelled_)throw std::runtime_error("Operation cancelled.");
    if(fs::file_size(archive)!=size||fs::last_write_time(archive)!=stamp)throw std::runtime_error("다른 프로그램이 압축 파일을 변경했습니다. 다시 열어 주세요.");
#ifdef _WIN32
    if(!ReplaceFileW(archive.c_str(),working.c_str(),nullptr,0,nullptr,nullptr))throw fs::filesystem_error("Replace archive",archive,std::error_code(GetLastError(),std::system_category()));
#else
    fs::rename(working,archive);
#endif
    std::error_code ec;fs::remove_all(staging,ec);
  }catch(...){std::error_code ec;fs::remove_all(staging,ec);throw;}
}
fs::path Engine::create(const Options& o, const fs::path& output, const std::vector<fs::path>& inputs) {
  createArgs(o,output,inputs);
  const auto requested=fs::absolute(output);
  for(const auto& input:inputs)if(fs::is_directory(input)){
    auto relative=fs::weakly_canonical(requested.parent_path()).lexically_relative(fs::weakly_canonical(input));
    if(relative.empty()||(*relative.begin()!=fs::path("..")&&!relative.is_absolute()))throw std::runtime_error("압축 대상 폴더 밖에 결과 파일을 저장하세요.");
  }
  // Compression staging is kept out of the source/output folder in OS TEMP.
  const auto staging=freshDirectory(fs::temp_directory_path());
  std::vector<std::pair<fs::path,fs::path>> moved;
  try {
    run(createArgs(o,staging/requested.filename(),inputs),o.format=="zpaq"?codec("zpaq"):fs::path{},1);
    std::vector<fs::path> volumes;for(const auto& entry:fs::directory_iterator(staging))volumes.push_back(entry.path());
    std::sort(volumes.begin(),volumes.end());
    if(volumes.empty())throw std::runtime_error("No archive was produced.");
    fs::path chosen;
    for(unsigned n=0;n<10000;++n){
      auto candidate=numbered(requested,n);bool available=!fs::exists(candidate);
      for(const auto& volume:volumes){
        auto suffix=volume.filename().u8string().substr(requested.filename().u8string().size());
        auto target=fs::u8path(candidate.u8string()+suffix);
        if(fs::exists(target))available=false;
      }
      if(available){chosen=candidate;break;}
    }
    if(chosen.empty())throw std::runtime_error("Cannot find an unused archive name.");
    for(const auto& volume:volumes){
      if(cancelled_)throw std::runtime_error("Operation cancelled.");
      auto target=fs::u8path(chosen.u8string()+volume.filename().u8string().substr(requested.filename().u8string().size()));
      moveExclusive(volume,target);moved.emplace_back(target,volume);
    }
    std::error_code ec;fs::remove(staging,ec);
    return o.split.empty()?chosen:fs::u8path(chosen.u8string()+".001");
  }catch(...){
    for(auto it=moved.rbegin();it!=moved.rend();++it){try{moveExclusive(it->first,it->second);}catch(...){}}
    std::error_code ec;fs::remove_all(staging,ec);throw;
  }
}
std::vector<fs::path> Engine::createEach(const std::vector<fs::path>& inputs) {
  if(inputs.empty())throw std::runtime_error("압축할 파일이 없습니다.");
  std::vector<fs::path> results;
  for(const auto& input:inputs)try{results.push_back(createQuick({input}));}
  catch(const std::exception& e){throw std::runtime_error(std::to_string(results.size())+"개 완료 후 중단: "+input.u8string()+"\n"+e.what());}
  return results;
}
std::vector<fs::path> Engine::extractEach(const std::vector<fs::path>& inputs,const std::string& password) {
  if(inputs.empty())throw std::runtime_error("해제할 파일이 없습니다.");
  std::vector<fs::path> results;
  for(const auto& input:inputs)try{results.push_back(extractSmart(input,fs::absolute(input).parent_path(),password,true));}
  catch(const std::exception& e){throw std::runtime_error(std::to_string(results.size())+"개 완료 후 중단: "+input.u8string()+"\n"+e.what());}
  return results;
}fs::path Engine::decryptEasyCrypt(const fs::path& input,const fs::path& parent,const std::string& password) {
  passwordCheck(password);
  if(password.empty())throw std::runtime_error("비밀번호를 입력하세요.");
  if(archiveExtension(input)!=".ezc"||!fs::is_regular_file(input))throw std::runtime_error("EasyCrypt .ezc 파일을 선택하세요.");
  if(!fs::is_directory(parent))throw std::runtime_error("대상 폴더가 존재하지 않습니다.");
  const auto name=input.stem();
  if(name.empty()||name=="."||name=="..")throw std::runtime_error("잘못된 출력 파일 이름입니다.");
  auto staging=freshDirectory(parent);
  try{
    const auto decoded=staging/name;
    run({fs::absolute(input).u8string(),password,"--output",fs::absolute(decoded).u8string()},codec("easycrab"),2);
    if(cancelled_)throw std::runtime_error("Operation cancelled.");
    fs::path output;
    for(unsigned n=0;n<10000;++n){auto candidate=numbered(parent/name,n);if(!fs::exists(candidate)){output=candidate;break;}}
    if(output.empty())throw std::runtime_error("사용 가능한 출력 이름이 없습니다.");
    moveExclusive(decoded,output);std::error_code ec;fs::remove(staging,ec);return output;
  }catch(...){std::error_code ec;fs::remove_all(staging,ec);throw;}
}fs::path Engine::createQuick(const std::vector<fs::path>& inputs) {
  if(inputs.empty())throw std::runtime_error("압축할 파일이 없습니다.");
  Options options;options.level=1;options.multiThread=parallel_;
  return create(options,suggestedArchive(inputs),inputs);
}
fs::path Engine::extract(const fs::path& archive,const fs::path& parent,const std::string& password,const std::vector<std::string>& selected) {
  auto entries=list(archive,password);validateEntries(entries);
  if(!selected.empty()){
    std::vector<Entry> filtered;
    auto normalize=[](std::string name){std::replace(name.begin(),name.end(),'\\','/');while(!name.empty()&&name.back()=='/')name.pop_back();return name;};
    std::vector<bool> matched(selected.size(),false);
    for(const auto& entry:entries){
      const auto path=normalize(entry.name);bool include=false;
      for(size_t i=0;i<selected.size();++i){
        const auto target=normalize(selected[i]);
        if(!target.empty()&&(path==target||(path.size()>target.size()&&path.compare(0,target.size(),target)==0&&path[target.size()]=='/'))){include=true;matched[i]=true;}
      }
      if(include)filtered.push_back(entry);
    }
    if(std::find(matched.begin(),matched.end(),false)!=matched.end())throw std::runtime_error("선택한 항목을 압축 파일에서 찾을 수 없습니다.");
    entries=std::move(filtered);
  }
  if(!fs::is_directory(parent))throw std::runtime_error("대상 폴더가 존재하지 않습니다.");
  auto staging=freshDirectory(parent);
  fs::path selectionDirectory;
  std::vector<std::pair<fs::path,fs::path>> moved;
  try {
    const auto format=archiveExtension(archive);
    if(format==".alz"||format==".egg"||format==".zpaq"){
      std::vector<std::string> args;
      if(format==".zpaq"){
        args={"x",fs::absolute(archive).u8string(),"-to",staging.u8string(),"-threads",parallel_?"0":"1"};
        if(!password.empty())args.insert(args.end(),{"-key",password});
      }else{
        if(password.empty()&&std::any_of(entries.begin(),entries.end(),[](const Entry& e){return e.encrypted;}))throw PasswordRequired("암호를 입력한 뒤 다시 시도하세요.");
        args={"-q","-d",staging.u8string()};
        if(!password.empty())args.insert(args.end(),{"-P",password});
        args.insert(args.end(),{"--",fs::absolute(archive).u8string()});
      }
      run(args,codec(format==".alz"?"unalz":format==".egg"?"unegg":"zpaq"),2);
      // Decode all to private staging, then retain selected files only. This also
      // supports solid EGG streams without requiring partial codec restarts.
      if(!selected.empty()){
        std::set<std::string> keep;
        for(const auto& entry:entries){auto name=entry.name;std::replace(name.begin(),name.end(),'\\','/');while(!name.empty()&&name.back()=='/')name.pop_back();keep.insert(name);
          auto slash=name.rfind('/');while(slash!=std::string::npos){name.resize(slash);keep.insert(name);slash=name.rfind('/');}}
        std::vector<fs::path> discard;
        for(const auto& entry:fs::recursive_directory_iterator(staging))if(!keep.count(entry.path().lexically_relative(staging).generic_u8string()))discard.push_back(entry.path());
        std::sort(discard.rbegin(),discard.rend());
        for(const auto& path:discard){std::error_code ec;fs::remove(path,ec);}
      }
    }else{
    std::vector<std::string> args={"x","-bsp1",parallel_?"-mmt=on":"-mmt=off","-y","-sccUTF-8","-o"+staging.u8string()};
    auto ext=archive.extension().u8string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(codePage_&&ext==".zip")args.push_back("-mcp="+std::to_string(codePage_));
    if(!password.empty())args.push_back("-p"+password);
    if(!selected.empty()){
      selectionDirectory=freshDirectory(parent);
      auto listFile=selectionDirectory/"selected.txt";
      std::ofstream output(listFile,std::ios::binary);
      for(const auto& entry:entries)output<<entry.name<<"\n";
      output.close();if(!output)throw std::runtime_error("선택 목록을 기록할 수 없습니다.");
      args.insert(args.end(),{"-spd","-scsUTF-8","-i@"+listFile.u8string()});
    }
    args.insert(args.end(),{"--",fs::absolute(archive).u8string()});

    run(args);
    }
#ifdef _WIN32
    if(propagateZone_){
      std::ifstream zone(fs::path(archive.wstring()+L":Zone.Identifier"),std::ios::binary);
      if(zone){
        std::string metadata;char buffer[4096];while(zone.read(buffer,sizeof(buffer))||zone.gcount()){metadata.append(buffer,static_cast<size_t>(zone.gcount()));if(metadata.size()>65536)throw std::runtime_error("다운로드 출처 정보가 너무 큽니다.");}
        for(const auto& entry:fs::recursive_directory_iterator(staging))if(entry.is_regular_file()){
          std::ofstream output(fs::path(entry.path().wstring()+L":Zone.Identifier"),std::ios::binary);
          output.write(metadata.data(),static_cast<std::streamsize>(metadata.size()));
          if(!output)throw std::runtime_error("다운로드 출처 정보를 저장할 수 없습니다.");
        }
      }
    }
#endif
    if(!selectionDirectory.empty()){fs::remove_all(selectionDirectory);selectionDirectory.clear();}
    // Verify decoded output before publishing, including auxiliary codecs.
    for(const auto& file:fs::recursive_directory_iterator(staging)){
      if(file.is_symlink()||(!file.is_directory()&&!file.is_regular_file()))throw std::runtime_error("Unsafe extracted file type.");
    }
    for(const auto& entry:entries)if(!entry.directory){
      auto name=entry.name;std::replace(name.begin(),name.end(),'\\','/');
      auto output=staging/fs::u8path(name);
      if(!fs::is_regular_file(output)||fs::file_size(output)!=entry.size)throw std::runtime_error("Incomplete extracted file: "+name);
    }
    std::vector<fs::path> children;for(const auto& entry:fs::directory_iterator(staging))children.push_back(entry.path());
    for(const auto& child:children){
      if(cancelled_)throw std::runtime_error("Operation cancelled.");
      // Publish without replacing existing files, directories or dangling links.
      // Exclusive moves also protect against another process creating the name.
      bool published=false;
      for(unsigned n=0;n<10000;++n){
        auto target=parent/child.filename();
        if(n)target=fs::is_directory(child)
          ?parent/fs::u8path(child.filename().u8string()+" ("+std::to_string(n)+")")
          :numbered(target,n);
        if(fs::symlink_status(target).type()!=fs::file_type::not_found)continue;
        try{moveExclusive(child,target);moved.emplace_back(target,child);published=true;break;}
        catch(const fs::filesystem_error&){
          if(fs::symlink_status(target).type()!=fs::file_type::not_found)continue;
          throw;
        }
      }
      if(!published)throw std::runtime_error("Cannot find an unused extracted name.");
    }
    std::error_code ec;fs::remove(staging,ec);return parent;
  }catch(...){
    for(auto it=moved.rbegin();it!=moved.rend();++it){try{moveExclusive(it->first,it->second);}catch(...){}}
    std::error_code ec;if(!selectionDirectory.empty())fs::remove_all(selectionDirectory,ec);fs::remove_all(staging,ec);throw;
  }
}
fs::path Engine::extractSmart(const fs::path& archive,const fs::path& parent,const std::string& password,bool forceFolder) {
  const auto entries=list(archive,password);validateEntries(entries);
  if(!fs::is_directory(parent))throw std::runtime_error("대상 폴더가 존재하지 않습니다.");
  std::string top;bool singleFolder=!entries.empty();
  for(const auto& entry:entries){
    auto name=entry.name;std::replace(name.begin(),name.end(),'\\','/');
    while(!name.empty()&&name.back()=='/')name.pop_back();
    auto slash=name.find('/');auto first=name.substr(0,slash);
    if(top.empty())top=first;
    if(first!=top||(slash==std::string::npos&&!entry.directory))singleFolder=false;
  }
  if(singleFolder&&!forceFolder&&!fs::exists(parent/fs::u8path(top)))return extract(archive,parent,password);
  auto name=archive.stem();
  if(archive.extension()==".001")name=name.stem();
  if(name.extension()==".tar")name=name.stem();
  if(name.empty()||name=="."||name=="..")name="Archive";
  const auto desired=parent/name;fs::path target;
  for(unsigned n=0;n<10000;++n){
    auto candidate=n?parent/fs::u8path(name.u8string()+" ("+std::to_string(n)+")"):desired;
    if(fs::create_directory(candidate)){target=candidate;break;}
  }
  if(target.empty())throw std::runtime_error("새 해제 폴더를 만들 수 없습니다.");
  try{return extract(archive,target,password);}
  catch(...){std::error_code ec;fs::remove(target,ec);throw;}
}}