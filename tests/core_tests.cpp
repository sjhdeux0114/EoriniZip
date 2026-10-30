#include "core.hpp"
#include "archive_name.hpp"
#include "split_presets.hpp"
#include <iostream>
#include <fstream>
#include <random>
#include <stdexcept>
#include <algorithm>
#include <thread>
#include <chrono>
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
        auto archive=destination;check(archive.parent_path()==root,"Archive must be saved directly");
        auto entries=engine.list(archive,opt.password);
        check(entries.size()==1 && entries[0].name=="한글.txt","Real listing");
        engine.test(archive,opt.password);
        if(!opt.password.empty()) rejects([&]{engine.test(archive,"wrong");});
        auto target=root/(std::string("extract-")+format);kz::fs::create_directory(target);
        auto extracted=engine.extract(archive,target,opt.password);check(extracted==target,"Extract directly to chosen folder");
        rejects([&]{engine.extract(archive,target,opt.password);});
        std::ifstream f(extracted/input.filename(),std::ios::binary); std::string actual((std::istreambuf_iterator<char>(f)),{});
        check(actual==content,"Roundtrip differs");
        auto second=engine.create(opt,root/filename,{input}); check(second!=destination,"Never overwrite");
      }
      auto randomFile=root/"random.bin"; {std::ofstream f(randomFile,std::ios::binary); std::mt19937 gen(42); for(int i=0;i<100000;++i)f.put(static_cast<char>(gen()));}
      kz::Options split;split.format="7z";split.split="10k";
      auto destination=engine.create(split,root/"parts.7z",{randomFile});
      check(kz::fs::exists(root/"parts.7z.002"),"Split volumes missing"); engine.test(destination);
      auto large=root/"preset.bin";
      {std::ofstream f(large,std::ios::binary); std::mt19937 gen(43);
       std::string block(65536,'\0');for(int i=0;i<100;++i){for(auto& c:block)c=static_cast<char>(gen());f.write(block.data(),block.size());}}
      for(auto format:{"zip","7z"}) {
        kz::Options preset;preset.format=format;preset.level=0;preset.split=kz::splitArgument(1);
        const auto base=std::string("preset.")+format;
        auto dest=engine.create(preset,root/base,{large});
        check(kz::fs::file_size(dest)==5ULL*1024*1024,"5MB preset first volume size");
        check(kz::fs::exists(root/(base+".002")),"Preset second volume");
        engine.test(dest);
        auto target=root/(std::string("preset-extract-")+format);kz::fs::create_directory(target);auto unpacked=engine.extract(dest,target);
        std::ifstream original(large,std::ios::binary),roundtrip(unpacked/large.filename(),std::ios::binary);
        check(std::string(std::istreambuf_iterator<char>(original),{})==std::string(std::istreambuf_iterator<char>(roundtrip),{}),"Preset split roundtrip");
      }
      check(kz::suggestedArchive({input}).filename()==kz::fs::u8path("한글.zip"),"Single file name");
      auto folder=root/"my.folder";kz::fs::create_directory(folder);
      check(kz::suggestedArchive({folder}).filename()=="my.folder.zip","Directory dots preserved");
      check(kz::suggestedArchive({input,large}).filename()==kz::fs::path(root.filename().u8string()+".zip"),"Multiple input folder name");
      auto quick=engine.createQuick({input});check(quick==root/kz::fs::u8path("한글.zip"),"Quick output path");engine.test(quick);
      auto again=engine.createQuick({input});check(again==root/kz::fs::u8path("한글 (1).zip"),"Quick collision protection");engine.test(quick);
      for(const auto& entry:kz::fs::directory_iterator(root))check(entry.path().filename().u8string().find(".eorinizip-tmp-")!=0,"No random result folders remain");
      kz::Options metadata;metadata.excludeThumbs=true;metadata.multiThread=false;
      auto thumb=root/"Thumbs.db";{std::ofstream f(thumb);f<<"thumbnail cache";}
      auto filtered=engine.create(metadata,root/"filtered.zip",{input,thumb});
      auto filteredEntries=engine.list(filtered);check(filteredEntries.size()==1&&filteredEntries[0].name=="한글.txt","Thumbs.db excluded");
      auto metadataArgs=kz::createArgs(metadata,root/"test.zip",{input});
      for(const auto& flag:{"-mcu=on","-mtc=on","-mta=on","-mmt=off","-xr!Thumbs.db"})
        check(std::find(metadataArgs.begin(),metadataArgs.end(),flag)!=metadataArgs.end(),"Configured creation option");
#ifdef _WIN32
      {std::ofstream zone(kz::fs::path(filtered.wstring()+L":Zone.Identifier"));zone<<"[ZoneTransfer]\r\nZoneId=3\r\n";}
      auto zoned=root/"zoned";kz::fs::create_directory(zoned);engine.configureRead(949,true,true);engine.extract(filtered,zoned);
      std::ifstream zone(kz::fs::path((zoned/input.filename()).wstring()+L":Zone.Identifier"));
      std::string zoneText((std::istreambuf_iterator<char>(zone)),{});check(zoneText.find("ZoneId=3")!=std::string::npos,"Download origin preserved");
      engine.configureRead(0,true,true);
#endif
      check(engine.progress()==100,"Completion progress");
      kz::Options progressOptions;progressOptions.format="7z";progressOptions.level=9;
      std::atomic<bool> done{false};std::exception_ptr failure;bool sawLiveProgress=false;
      std::thread job([&]{try{engine.create(progressOptions,root/"progress.7z",{large});}catch(...){failure=std::current_exception();}done=true;});
      while(!done){int progress=engine.progress();if(engine.phase()==1&&progress>=0&&progress<100)sawLiveProgress=true;std::this_thread::sleep_for(std::chrono::milliseconds(5));}
      job.join();if(failure)std::rethrow_exception(failure);check(sawLiveProgress,"Live engine progress parsing");
      done=false;failure=nullptr;bool cancelledLive=false;
      std::thread cancelling([&]{try{engine.create(progressOptions,root/"cancelled.7z",{large});}catch(...){failure=std::current_exception();}done=true;});
      while(!done){
        if(engine.phase()==1&&engine.progress()>=0&&engine.progress()<100){engine.cancel();cancelledLive=true;break;}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      cancelling.join();check(cancelledLive&&failure,"Cancel an active engine");
      check(!kz::fs::exists(root/"cancelled.7z"),"Cancelled archive not published");
      for(const auto& entry:kz::fs::directory_iterator(root))check(entry.path().filename().u8string().find(".eorinizip-tmp-")!=0,"Cancelled staging cleaned");
      engine.resetCancel();
      engine.cancel();rejects([&]{engine.list(destination);});engine.resetCancel();
      engine.test(destination);
      std::cout<<"ZIP/7Z/TAR + password + split + no-overwrite + cancellation checks passed\n";
    } catch(...) {kz::fs::remove_all(root);throw;}
    kz::fs::remove_all(root); return 0;
  } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
