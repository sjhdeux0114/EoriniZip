#include "core.hpp"
#include "split_presets.hpp"
#include <iostream>
#include <fstream>
#include <random>
#include <stdexcept>
#include <algorithm>
void check(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
template<class F> void rejects(F f) { bool failed=false; try{f();}catch(const std::exception&){failed=true;}check(failed,"Expected rejection"); }
int main(int argc,char** argv) {
  try {
    kz::Options o; o.password="p & $test"; o.split="100m";
    auto args=kz::createArgs(o,"out.zip",{"input.txt"});
    check(std::find(args.begin(),args.end(),"-mem=AES256")!=args.end(),"AES option");
    check(std::find(args.begin(),args.end(),"-pp & $test")!=args.end(),"Password must remain one argument");
    o.format="tar"; rejects([&]{kz::createArgs(o,"out.tar",{"input"});});
    auto rows=kz::parseListing("Path = 한글 = file.txt\nSize = 123\nPacked Size = 90\nEncrypted = +\n\nPath = folder\nFolder = +\nSize = 0\n");
    check(rows.size()==2 && rows[0].name=="한글 = file.txt" && rows[0].encrypted && rows[1].directory,"Listing parsing");
    for(const auto& name : {"../escape","..\\escape","/absolute","C:\\evil","file:stream","CON.txt","a/../b","a./b","a//b"}) {
      kz::Entry e; e.name=name; rejects([&]{kz::validateEntries({e});});
    }
    kz::Entry a; a.name="a"; kz::Entry b; b.name="A"; rejects([&]{kz::validateEntries({a,b});});
    a.link=true; rejects([&]{kz::validateEntries({a});});
    check(kz::quoteWindows(L"a b\\")==L"\"a b\\\\\"","Windows trailing slash quoting");
    check(kz::quoteWindows(L"a\"b")==L"\"a\\\"b\"","Windows quote escaping");
    check(kz::splitArgument(0).empty(),"No splitting preset");
    check(kz::splitArgument(18)=="4294967295b","FAT32 maximum file size");
    check(kz::splitArgument(kz::customSplitIndex,"2g")=="2g","Custom split");
    rejects([]{kz::splitArgument(kz::customSplitIndex);});
    rejects([]{kz::splitArgument(100);});
    for(size_t i=1;i<kz::customSplitIndex;++i) {
      kz::Options preset; preset.split=kz::splitArgument(i);
      kz::createArgs(preset,"out.zip",{"input"});
    }
    for(auto invalid:{"0m","-1m","1.5g","10m -y","99999999999999g"}) {
      kz::Options invalidOptions;invalidOptions.split=invalid;
      rejects([&]{kz::createArgs(invalidOptions,"out.zip",{"input"});});
    }
    std::cout<<"Core unit checks passed\n";
    if(argc<2) {std::cout<<"Integration SKIPPED: supply full 7-Zip executable path\n"; return 0;}
    auto root=kz::fs::temp_directory_path()/("eorinizip-native-test-"+std::to_string(std::random_device{}()));
    kz::fs::create_directory(root);
    try {
      auto input=root/kz::fs::u8path("한글.txt"); const std::string content="EoriniZip native round trip 한글";
      {std::ofstream file(input,std::ios::binary); file<<content;}
      kz::Engine engine(kz::fs::u8path(argv[1]));
      for(const auto& format : {"zip","7z","tar"}) {
        kz::Options opt; opt.format=format; if(opt.format!="tar")opt.password="test & password"; opt.hideNames=opt.format=="7z";
        auto filename=std::string("archive.")+format;
        auto destination=engine.create(opt,root/filename,{input});
        auto archive=destination/filename;
        auto entries=engine.list(archive,opt.password);
        check(entries.size()==1 && entries[0].name=="한글.txt","Real listing");
        engine.test(archive,opt.password);
        if(!opt.password.empty()) rejects([&]{engine.test(archive,"wrong");});
        auto extracted=engine.extract(archive,root,opt.password);
        std::ifstream f(extracted/input.filename(),std::ios::binary); std::string actual((std::istreambuf_iterator<char>(f)),{});
        check(actual==content,"Roundtrip differs");
        auto second=engine.create(opt,root/filename,{input}); check(second!=destination,"Never overwrite");
      }
      auto randomFile=root/"random.bin"; {std::ofstream f(randomFile,std::ios::binary); std::mt19937 gen(42); for(int i=0;i<100000;++i)f.put(static_cast<char>(gen()));}
      kz::Options split;split.format="7z";split.split="10k";
      auto destination=engine.create(split,root/"parts.7z",{randomFile});
      check(kz::fs::exists(destination/"parts.7z.002"),"Split volumes missing"); engine.test(destination/"parts.7z.001");
      auto large=root/"preset.bin";
      {std::ofstream f(large,std::ios::binary); std::mt19937 gen(43);
       std::string block(65536,'\0');for(int i=0;i<100;++i){for(auto& c:block)c=static_cast<char>(gen());f.write(block.data(),block.size());}}
      for(auto format:{"zip","7z"}) {
        kz::Options preset;preset.format=format;preset.level=0;preset.split=kz::splitArgument(1);
        const auto base=std::string("preset.")+format;
        auto dest=engine.create(preset,root/base,{large});
        check(kz::fs::file_size(dest/(base+".001"))==5ULL*1024*1024,"5MB preset first volume size");
        check(kz::fs::exists(dest/(base+".002")),"Preset second volume");
        engine.test(dest/(base+".001"));
        auto unpacked=engine.extract(dest/(base+".001"),root);
        std::ifstream original(large,std::ios::binary),roundtrip(unpacked/large.filename(),std::ios::binary);
        check(std::string(std::istreambuf_iterator<char>(original),{})==std::string(std::istreambuf_iterator<char>(roundtrip),{}),"Preset split roundtrip");
      }
      engine.cancel();rejects([&]{engine.list(destination/"parts.7z.001");});engine.resetCancel();
      engine.test(destination/"parts.7z.001");
      std::cout<<"ZIP/7Z/TAR + password + split + no-overwrite + cancellation checks passed\n";
    } catch(...) {kz::fs::remove_all(root);throw;}
    kz::fs::remove_all(root); return 0;
  } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
