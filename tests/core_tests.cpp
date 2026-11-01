#include "core.hpp"
#include "archive_name.hpp"
#include "split_presets.hpp"
#include "preferences.hpp"
#include "ui_language.hpp"
#include <iostream>
#include <fstream>
#include <random>
#include <stdexcept>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cstdlib>
#ifdef _WIN32
#include "shell_preview.hpp"
#endif
void check(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
template<class F> void rejects(F f) { bool failed=false; try{f();}catch(const std::exception&){failed=true;}check(failed,"Expected rejection"); }
int main(int argc,char** argv) {
  try {
    kz::ui::english=true;
    check(kz::ui::label(L"압축 파일 열기\tCtrl+O")==L"Open archive\tCtrl+O","English labels preserve keyboard shortcuts");
    check(kz::ui::label(L"C:\\한글\\압축 파일 열기.txt")==L"C:\\한글\\압축 파일 열기.txt","Localization preserves user file paths");
    kz::ui::english=false;check(kz::ui::label(L"압축 파일 열기")==L"압축 파일 열기","Korean labels preserved");
    const std::string decomposed=u8"\u1112\u1161\u11ab\u1100\u1173\u11af";
    check(kz::composeFilename(decomposed)==u8"한글","macOS decomposed Hangul composes to NFC");
    check(kz::composeFilename(u8"cafe\u0301.txt")==u8"café.txt","Accented Latin composition");
    check(kz::composeFilename(u8"😀 한글.txt")==u8"😀 한글.txt","Supplementary Unicode preserved");
    kz::Entry nfd,nfc;nfd.name=decomposed+".txt";nfc.name=u8"한글.txt";
    rejects([&]{kz::validateEntries({nfd,nfc});});
    nfd.name=u8"café.txt";nfc.name=u8"CAFÉ.TXT";rejects([&]{kz::validateEntries({nfd,nfc});});
    kz::Preferences prefs;
    for(const auto& field:kz::preferenceFields())prefs.*(field.member)=field.maximum;
    auto restored=kz::importPreferences(kz::exportPreferences(prefs));
    for(const auto& field:kz::preferenceFields())check(restored.*(field.member)==prefs.*(field.member),"Preferences roundtrip");
    auto defaults=kz::importPreferences("[EoriniZip]\r\nVersion=1\r\nGrid=1\r\n");
    check(defaults.grid==1&&defaults.escape==1&&defaults.level==2,"Missing preferences use defaults");
    for(auto invalid:{"","Version=1","[EoriniZip]\nVersion=2","[EoriniZip]\nVersion=1\nGrid=2","[EoriniZip]\nVersion=1\nLevel=-1",
      "[EoriniZip]\nVersion=1\nGrid=1\nGrid=0","[EoriniZip]\nVersion=1\nUnknown=1","[EoriniZip]\nVersion=1\nTheme=16777216",
      "[EoriniZip]\nVersion=1\nGrid=1junk","[EoriniZip]\nVersion=1\nCodePageIndex=7","[EoriniZip]\nGrid=0"})
      rejects([&]{kz::importPreferences(invalid);});
    rejects([]{kz::importPreferences(std::string(65537,'x'));});
    prefs.level=4;rejects([&]{kz::exportPreferences(prefs);});
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
    const char* testRoot=std::getenv("EORINIZIP_TEST_ROOT");
    auto root=(testRoot?kz::fs::u8path(testRoot):kz::fs::temp_directory_path())/("eorinizip-native-test-"+std::to_string(std::random_device{}()));
    kz::fs::create_directory(root);
    try {
      auto input=root/kz::fs::u8path("한글.txt"); const std::string content="EoriniZip native round trip 한글";
      {std::ofstream file(input,std::ios::binary); file<<content;}
      kz::Engine engine(kz::fs::u8path(argv[1]));
      {
        auto customTemp=root/"custom-temp";kz::fs::create_directory(customTemp);engine.configureTemporaryDirectory(customTemp);
        for(const auto& pair:std::vector<std::pair<std::string,std::string>>{{"gzip","gz"},{"bzip2","bz2"},{"xz","xz"},{"wim","wim"}}){
          kz::Options options;options.format=pair.first;
          auto output=engine.create(options,root/("single."+pair.second),{input});engine.test(output);
          auto destination=root/("single-"+pair.second);kz::fs::create_directory(destination);engine.extract(output,destination);
          check(!kz::fs::is_empty(destination),"Single stream extraction produced a file");
          for(const auto& entry:kz::fs::directory_iterator(destination)){std::ifstream file(entry.path(),std::ios::binary);std::string actual((std::istreambuf_iterator<char>(file)),{});check(actual==content,"Additional creation format bytes preserved");}
        }
        check(kz::fs::is_empty(customTemp),"Custom compression temp folder cleaned");engine.configureTemporaryDirectory({});
        auto isoSource=root/"iso-source";kz::fs::create_directory(isoSource);kz::fs::copy_file(input,isoSource/input.filename());
        kz::Options iso;iso.format="iso";auto image=engine.create(iso,root/"data.iso",{isoSource});engine.test(image);
        auto isoDestination=root/"iso-extracted";kz::fs::create_directory(isoDestination);engine.extract(image,isoDestination);
        check(kz::fs::exists(isoDestination/input.filename()),"ISO contains folder contents with Korean names");
        {std::ifstream file(isoDestination/input.filename(),std::ios::binary);std::string actual((std::istreambuf_iterator<char>(file)),{});check(actual==content,"ISO file content roundtrip");}
#ifdef _WIN32
        if(kz::fs::exists(kz::fs::u8path(argv[1]).parent_path()/"7z.sfx")){
          kz::Options sfx;sfx.format="7z";sfx.selfExtracting=true;sfx.password="sfx-password";sfx.hideNames=true;
          auto output=engine.create(sfx,root/"self-extracting.exe",{input});engine.test(output,sfx.password);
          auto entries=engine.list(output,sfx.password);check(entries.size()==1&&entries[0].name=="한글.txt","Official engine creates readable SFX with encrypted header");
        }else std::cout<<"SFX SKIPPED: official 7z.sfx missing\n";
#endif
      }
      {
        kz::Options options;options.level=0;auto original=engine.create(options,root/"repair-source.zip",{input});
        std::ifstream f(original,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(f)),{});auto central=bytes.find("PK\001\002");check(central!=std::string::npos,"ZIP central directory fixture");
        auto broken=root/"repair-damaged.zip";{std::ofstream truncated(broken,std::ios::binary);truncated.write(bytes.data(),static_cast<std::streamsize>(central));}
        auto originalBrokenSize=kz::fs::file_size(broken);auto repaired=engine.repairZip(broken,root);engine.test(repaired);
        auto recoveredEntries=engine.list(repaired);check(recoveredEntries.size()==1&&recoveredEntries[0].name=="한글.txt","Recover ZIP with missing central directory");
        check(kz::fs::file_size(broken)==originalBrokenSize,"ZIP recovery never modifies source");
        auto corrupt=root/"repair-corrupt.zip";bytes[central-1]^=0x7f;{std::ofstream damaged(corrupt,std::ios::binary);damaged.write(bytes.data(),static_cast<std::streamsize>(central));}
        rejects([&]{engine.repairZip(corrupt,root);});check(!kz::fs::exists(root/"repair-corrupt-recovered.zip"),"CRC failure does not publish recovered ZIP");
      }
      {
        // APFS/HFS+ resolve NFC and NFD spellings to the same file. Keep this
        // fixture separate so it cannot overwrite the main roundtrip input.
        auto macSource=root/"nfd-source";kz::fs::create_directory(macSource);
        auto macInput=macSource/kz::fs::u8path(decomposed+".txt");{std::ofstream f(macInput);f<<"macOS name";}
        kz::Options options;
        auto macArchive=engine.create(options,root/"mac-nfd.zip",{macInput});
        auto macEntries=engine.list(macArchive);check(macEntries.size()==1,"NFD archive listing");
        auto destination=root/"mac-normalized";kz::fs::create_directory(destination);
        engine.extract(macArchive,destination,"",{macEntries[0].name});
        check(kz::fs::exists(destination/kz::fs::u8path("한글.txt")),"NFD extraction yields usable NFC name");
        std::ifstream f(destination/kz::fs::u8path("한글.txt"));std::string value;std::getline(f,value);check(value=="macOS name","NFD content preserved");
        engine.extract(macArchive,destination);
        check(kz::fs::exists(destination/kz::fs::u8path("한글 (1).txt")),"NFD repeated extraction collision protection");
      }
      for(const auto& format : {"zip","7z","tar","zpaq"}) {
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
        engine.extract(archive,target,opt.password);
        engine.extract(archive,target,opt.password);
        for(const auto* name:{"한글 (1).txt","한글 (2).txt"}){
          std::ifstream numberedFile(target/kz::fs::u8path(name),std::ios::binary);
          std::string numberedContent((std::istreambuf_iterator<char>(numberedFile)),{});
          check(numberedContent==content,"Repeated extraction uses numbered names with identical content");
        }
        std::ifstream f(extracted/input.filename(),std::ios::binary); std::string actual((std::istreambuf_iterator<char>(f)),{});
        check(actual==content,"Roundtrip differs");
        auto second=engine.create(opt,root/filename,{input}); check(second!=destination,"Never overwrite");
      }
      for(const auto* format:{"zip","7z"}){
        kz::Options options;options.format=format;options.password="edit-test-password";options.hideNames=options.format=="7z";
        auto editable=engine.create(options,root/(std::string("editable.")+format),{input});
        check(engine.recoverPassword(editable,{"wrong-candidate",options.password})==options.password,"Password candidate recovery validates full archive");
        rejects([&]{engine.recoverPassword(editable,{"wrong-candidate"});});
        if(options.format=="zip"){
          check(engine.comment(editable).empty(),"New ZIP comment empty");
          engine.setComment(editable,u8"한글 ZIP 설명\nsecond line",options.password);
          check(engine.comment(editable)==u8"한글 ZIP 설명\nsecond line","UTF-8 ZIP comment roundtrip");
          auto commentStamp=kz::fs::last_write_time(editable);auto commentSize=kz::fs::file_size(editable);
          rejects([&]{engine.setComment(editable,"new comment","wrong");});
          check(kz::fs::last_write_time(editable)==commentStamp&&kz::fs::file_size(editable)==commentSize,"Failed comment edit preserves archive");
          rejects([&]{engine.setComment(editable,std::string(65536,'a'),options.password);});
        }
        auto extra=root/"extra.txt";{std::ofstream f(extra);f<<"added data";}
        engine.edit(editable,"a",{extra.u8string()},options.password);
        auto added=engine.list(editable,options.password);check(added.size()==2,"Add file to encrypted archive");
        check(std::all_of(added.begin(),added.end(),[](const auto& e){return e.encrypted;}),"Added files preserve encryption");
        engine.edit(editable,"rn",{"extra.txt","renamed.txt"},options.password);
        {std::ofstream f(extra);f<<"external editor replacement";}
        engine.edit(editable,"replace",{"renamed.txt",extra.u8string()},options.password);
        auto replaced=root/(std::string("replaced-")+format);kz::fs::create_directory(replaced);
        engine.extract(editable,replaced,options.password,{"renamed.txt"});
        {std::ifstream f(replaced/"renamed.txt");std::string value;std::getline(f,value);check(value=="external editor replacement","External edit replace preserves saved bytes");}
        rejects([&]{engine.edit(editable,"replace",{"../escape",extra.u8string()},options.password);});
        auto edited=engine.list(editable,options.password);
        check(std::any_of(edited.begin(),edited.end(),[](const auto& e){return e.name=="renamed.txt";}),"Rename internal file");
        auto before=kz::fs::file_size(editable);auto stamp=kz::fs::last_write_time(editable);
        rejects([&]{engine.edit(editable,"rn",{"renamed.txt","../outside.txt"},options.password);});
        rejects([&]{engine.edit(editable,"d",{"renamed.txt"},"wrong");});
        check(kz::fs::file_size(editable)==before&&kz::fs::last_write_time(editable)==stamp,"Failed edit preserves original archive");
        engine.edit(editable,"d",{"renamed.txt"},options.password);
        check(engine.list(editable,options.password).size()==1,"Delete internal file");engine.test(editable,options.password);
        auto nestedSource=root/(std::string("nested-source-")+format);kz::fs::create_directory(nestedSource);kz::fs::copy_file(input,nestedSource/input.filename());
        auto nested=engine.create(options,root/(std::string("nested-edit.")+format),{nestedSource});auto nestedName=nestedSource.filename().u8string()+"/한글.txt";
        engine.edit(nested,"replace",{nestedName,extra.u8string()},options.password);
        auto nestedOut=root/(std::string("nested-output-")+format);kz::fs::create_directory(nestedOut);engine.extract(nested,nestedOut,options.password);
        {std::ifstream file(nestedOut/kz::fs::u8path(nestedName));std::string value;std::getline(file,value);check(value=="external editor replacement","External edit preserves nested archive path");}
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
      kz::Options xzOptions;xzOptions.format="xz";xzOptions.multiThread=true;
      auto xzArgs=kz::createArgs(xzOptions,root/"test.xz",{input});
      check(std::find(xzArgs.begin(),xzArgs.end(),"-mmt=on")!=xzArgs.end(),"XZ multi-thread flag applied");
#ifdef _WIN32
      {std::ofstream zone(kz::fs::path(filtered.wstring()+L":Zone.Identifier"));zone<<"[ZoneTransfer]\r\nZoneId=3\r\n";}
      auto zoned=root/"zoned";kz::fs::create_directory(zoned);engine.configureRead(949,true,true);engine.extract(filtered,zoned);
      std::ifstream zone(kz::fs::path((zoned/input.filename()).wstring()+L":Zone.Identifier"));
      std::string zoneText((std::istreambuf_iterator<char>(zone)),{});check(zoneText.find("ZoneId=3")!=std::string::npos,"Download origin preserved");
      engine.configureRead(0,true,true);
#endif
      if(argc>2){
        const auto fixtures=kz::fs::u8path(argv[2]);
        struct Fixture{const char* engine;const char* name;const char* password;};
        const Fixture samples[]={
          {"unalz","store_empty.alz",""},{"unalz","deflate_empty.alz",""},{"unalz","bzip2_dlz.alz",""},
          {"unalz","enc_deflate.alz","test1234"},{"unalz","split_store.alz",""},
          {"unegg","deflate5.egg",""},{"unegg","azo_pe.egg",""},{"unegg","enc_aes256.egg","test1234"},
          {"unegg","enc_zip20.egg","test1234"}
        };
        unsigned index=0;
        for(const auto& sample:samples){
          const auto file=fixtures/sample.engine/"tests"/"data"/sample.name;
          auto rows=engine.list(file,sample.password);check(!rows.empty(),"ALZ/EGG native listing");
          auto out=root/("codec-"+std::to_string(index++));kz::fs::create_directory(out);
          engine.extract(file,out,sample.password);
          for(const auto& entry:rows)if(!entry.directory){auto name=entry.name;std::replace(name.begin(),name.end(),'\\','/');
            check(kz::fs::file_size(out/kz::fs::u8path(name))==entry.size,"ALZ/EGG extracted size");}
          engine.test(file,sample.password);
          if(*sample.password){auto bad=root/("bad-codec-"+std::to_string(index));kz::fs::create_directory(bad);
            rejects([&]{engine.extract(file,bad,"wrong-password");});check(kz::fs::is_empty(bad),"Wrong codec password leaves no files");}
        }
        std::cout<<"ALZ/EGG store, deflate, bzip2, AZO, encryption and split fixtures passed\n";
      }
      if(argc>4){
        const char* secret=std::getenv("EORINIZIP_TEST_PASSWORD");check(secret!=nullptr,"Private fixture password environment variable");
        auto encrypted=kz::fs::u8path(argv[3]),original=kz::fs::u8path(argv[4]);
        auto out=root/"easycrypt";kz::fs::create_directory(out);
        auto decoded=engine.decryptEasyCrypt(encrypted,out,secret);
        std::ifstream expected(original,std::ios::binary),actual(decoded,std::ios::binary);
        check(std::string((std::istreambuf_iterator<char>(expected)),{})==std::string((std::istreambuf_iterator<char>(actual)),{}),"Real EasyCrypt plaintext byte equality");
        auto bad=root/"easycrypt-bad";kz::fs::create_directory(bad);
        rejects([&]{engine.decryptEasyCrypt(encrypted,bad,"wrong-password");});check(kz::fs::is_empty(bad),"Wrong password leaves no output");
        auto corrupt=root/"corrupt.ezc";kz::fs::copy_file(encrypted,corrupt);
        {std::fstream f(corrupt,std::ios::in|std::ios::out|std::ios::binary);f.seekg(200);char c;f.get(c);c^=1;f.seekp(200);f.put(c);}
        rejects([&]{engine.decryptEasyCrypt(corrupt,bad,secret);});check(kz::fs::is_empty(bad),"Corrupt ciphertext is never published");
        auto shortFile=root/"short.ezc";{std::ofstream f(shortFile);f<<"EZC";}
        rejects([&]{engine.decryptEasyCrypt(shortFile,bad,secret);});check(kz::fs::is_empty(bad),"Truncated file leaves no output");
        auto again=engine.decryptEasyCrypt(encrypted,out,secret);check(again!=decoded,"EasyCrypt never overwrites");
        std::cout<<"Real EasyCrypt fixture: exact plaintext, wrong-password, corruption, truncation and no-overwrite checks passed\n";
      }
      auto batchSource=root/"batch";kz::fs::create_directory(batchSource);
      auto first=batchSource/"one.txt",second=batchSource/"two.txt";{std::ofstream(first)<<"one";std::ofstream(second)<<"two";}
      auto batchArchives=engine.createEach({first,second});check(batchArchives.size()==2,"Create each count");
      check(batchArchives[0].filename()=="one.zip"&&batchArchives[1].filename()=="two.zip","Create each file name");
      auto batchOutputs=engine.extractEach(batchArchives);check(batchOutputs.size()==2,"Extract each count");
      check(kz::fs::exists(batchOutputs[0]/"one.txt")&&!kz::fs::exists(batchOutputs[0]/"two.txt")&&kz::fs::exists(batchOutputs[1]/"two.txt"),"Separate extraction folders");
      rejects([&]{engine.createEach({});});rejects([&]{engine.extractEach({});});
#ifdef _WIN32
      auto preview=kz::shell::preview(root/"archive.zip");check(preview.size()==1&&preview[0].name==L"한글.txt","Native ZIP context preview name");
      check(kz::shell::preview(input).empty(),"Non-ZIP preview safely empty");
#endif
      bool passwordRequest=false;try{engine.list(root/"archive.7z");}catch(const kz::PasswordRequired&){passwordRequest=true;}check(passwordRequest,"Encrypted header requests password");
      passwordRequest=false;try{engine.test(root/"archive.zip","incorrect");}catch(const kz::PasswordRequired&){passwordRequest=true;}check(passwordRequest,"Wrong ZIP password requests retry");
      auto sourceTree=root/"selection-source";kz::fs::create_directories(sourceTree/"nested");
      {std::ofstream f(sourceTree/"nested"/"child.txt");f<<"chosen";}
      {std::ofstream f(sourceTree/"sibling.txt");f<<"not chosen";}
      kz::Options plain;auto nestedZip=engine.create(plain,root/"nested.zip",{sourceTree});
      auto selectionOut=root/"selection-out";kz::fs::create_directory(selectionOut);
      engine.extract(nestedZip,selectionOut,"",{"selection-source/nested/child.txt"});
      check(kz::fs::is_regular_file(selectionOut/"selection-source"/"nested"/"child.txt"),"Selected child extracted");
      check(!kz::fs::exists(selectionOut/"selection-source"/"sibling.txt"),"Unselected sibling excluded");
      auto dirOut=root/"directory-out";kz::fs::create_directory(dirOut);
      engine.extract(nestedZip,dirOut,"",{"selection-source/nested"});
      check(kz::fs::is_regular_file(dirOut/"selection-source"/"nested"/"child.txt"),"Selected directory descendants");
      rejects([&]{engine.extract(nestedZip,dirOut,"",{"missing"});});
      engine.extract(nestedZip,dirOut,"",{"selection-source/nested"});
      check(kz::fs::is_regular_file(dirOut/"selection-source (1)"/"nested"/"child.txt"),"Existing folder is numbered without changing its contents");
      auto originalSize=kz::fs::file_size(dirOut/"selection-source"/"nested"/"child.txt");
      check(originalSize==6,"Original folder content preserved");
      auto smartOut=root/"smart-out";kz::fs::create_directory(smartOut);
      check(engine.extractSmart(nestedZip,smartOut)==smartOut,"Single root folder uses destination directly");
      check(kz::fs::exists(smartOut/"selection-source"/"nested"/"child.txt"),"Smart nested contents");
      auto collision=engine.extractSmart(nestedZip,smartOut);
      check(collision==smartOut/"nested","Smart collision gets archive folder");
      auto collision2=engine.extractSmart(nestedZip,smartOut);
      check(collision2==smartOut/"nested (1)","Smart repeated output never overwrites");
      auto singleOut=root/"single-smart";kz::fs::create_directory(singleOut);
      auto singleResult=engine.extractSmart(filtered,singleOut);
      check(singleResult==singleOut/"filtered"&&kz::fs::exists(singleResult/input.filename()),"Loose file uses archive folder");
      auto unusual=root/"@literal[1].txt";{std::ofstream f(unusual);f<<"literal";}
      auto unusualZip=engine.create(plain,root/"literal.zip",{unusual,input});
      auto unusualOut=root/"literal-out";kz::fs::create_directory(unusualOut);
      engine.extract(unusualZip,unusualOut,"",{"@literal[1].txt"});
      check(kz::fs::exists(unusualOut/unusual.filename())&&!kz::fs::exists(unusualOut/input.filename()),"At-sign selection is literal, never an engine list file");
      auto wrongOut=root/"wrong-smart";kz::fs::create_directory(wrongOut);
      rejects([&]{engine.extractSmart(root/"archive.zip",wrongOut,"wrong",true);});
      check(kz::fs::is_empty(wrongOut),"Smart failure leaves no output folders");

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
      std::cout<<"ZIP/7Z/TAR + password + split + selected/smart extraction + no-overwrite + cancellation checks passed\n";
    } catch(...) {kz::fs::remove_all(root);throw;}
    kz::fs::remove_all(root); return 0;
  } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
