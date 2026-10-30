#include "core.hpp"
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <algorithm>
void check(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
template<class F> void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"Incomplete volume set must fail");}
int main(int argc,char** argv){
 if(argc!=4)return 2;
 auto root=kz::fs::temp_directory_path()/("eorinizip-split-"+std::to_string(std::random_device{}()));kz::fs::create_directory(root);
 try{
  kz::Engine engine(kz::fs::u8path(argv[1]));
  struct Set{std::string label;kz::fs::path source;std::string prefix,first,middle;};
  auto third=kz::fs::u8path(argv[2]),rar=kz::fs::u8path(argv[3]);
  std::vector<Set> sets={
   {"ALZ",third/"unalz/tests/data","split_store.","split_store.alz","split_store.a00"},
   {"EGG",third/"unegg/tests/data","split_store.vol","split_store.vol1.egg","split_store.vol2.egg"},
   {"RAR4",rar,"test_read_format_rar_multivolume.part","test_read_format_rar_multivolume.part0001.rar","test_read_format_rar_multivolume.part0002.rar"},
   {"RAR4-single",rar,"test_rar_multivolume_single_file.part","test_rar_multivolume_single_file.part1.rar","test_rar_multivolume_single_file.part2.rar"},
   {"RAR5-solid",rar,"test_read_format_rar5_multiarchive_solid.part","test_read_format_rar5_multiarchive_solid.part01.rar","test_read_format_rar5_multiarchive_solid.part02.rar"}};
  auto source=root/"payload.bin";std::mt19937 random(42);{std::ofstream out(source,std::ios::binary);for(int i=0;i<400000;++i)out.put(static_cast<char>(random()&255));}
  for(auto format:{"zip","7z"}){
   auto dir=root/(std::string("generated-")+format);kz::fs::create_directory(dir);kz::Options o;o.format=format;o.split="64k";o.password="split-test";
   auto first=engine.create(o,dir/(std::string("payload.")+format),{source});sets.push_back({format,dir,"payload.",first.filename().u8string(),std::string("payload.")+format+".002"});
  }
  for(const auto& set:sets){
   auto dir=root/set.label;kz::fs::create_directory(dir);
   for(const auto& f:kz::fs::directory_iterator(set.source))if(f.is_regular_file()&&f.path().filename().u8string().find(set.prefix)==0)kz::fs::copy_file(f.path(),dir/f.path().filename());
   auto first=dir/set.first,middle=dir/set.middle;check(kz::fs::exists(first)&&kz::fs::exists(middle),"Fixture volumes present");
   const std::string pw=(set.label=="zip"||set.label=="7z")?"split-test":"";
   if(set.label=="RAR4"){auto rejected=root/"link-rejected";kz::fs::create_directory(rejected);rejects([&]{engine.extract(first,rejected);});check(kz::fs::is_empty(rejected),"Split symlink archive rejected");std::cout<<"RAR4 symlink safety rejection passed\n";continue;}
   auto entries=engine.list(first,pw);check(!entries.empty(),"Split listing");
   auto out=root/(set.label+"-ok");kz::fs::create_directory(out);engine.extract(first,out,pw);engine.test(first,pw);
   for(const auto& e:entries)if(!e.directory){auto name=e.name;std::replace(name.begin(),name.end(),'\\','/');check(kz::fs::file_size(out/kz::fs::u8path(name))==e.size,"Split extracted size");}
   if(!pw.empty()){std::ifstream a(source,std::ios::binary),b(out/"payload.bin",std::ios::binary);check(std::string((std::istreambuf_iterator<char>(a)),{})==std::string((std::istreambuf_iterator<char>(b)),{}),"Encrypted split exact content");}
   auto failed=root/(set.label+"-failed");kz::fs::create_directory(failed);auto saved=root/(set.label+"-missing-volume.saved");kz::fs::rename(middle,saved);
   rejects([&]{engine.extract(first,failed,pw);});check(kz::fs::is_empty(failed),"Missing volume publishes no partial output");kz::fs::rename(saved,middle);
   const auto length=kz::fs::file_size(middle);{std::ofstream bad(middle,std::ios::binary|std::ios::trunc);std::string zeros(static_cast<size_t>(length),'\0');bad.write(zeros.data(),static_cast<std::streamsize>(zeros.size()));}
   rejects([&]{engine.extract(first,failed,pw);});check(kz::fs::is_empty(failed),"Corrupt volume publishes no partial output");
   std::cout<<set.label<<": complete, missing middle, corrupt middle passed\n";
  }
  kz::fs::remove_all(root);return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";kz::fs::remove_all(root);return 1;}
}
