#import <Cocoa/Cocoa.h>
#include "core.hpp"
#include "split_presets.hpp"
#include "archive_name.hpp"
#include <memory>
#include <functional>
static NSString* S(const std::string& s) { return [[[NSString alloc] initWithBytes:s.data() length:s.size() encoding:NSUTF8StringEncoding] precomposedStringWithCanonicalMapping] ?: @"Invalid UTF-8"; }
@interface YellowHomeView : NSView
@end
@implementation YellowHomeView
- (void)drawRect:(NSRect)rect {
  (void)rect;
  NSGradient* gradient=[[NSGradient alloc] initWithStartingColor:[NSColor colorWithSRGBRed:1 green:0.902 blue:0.51 alpha:1] endingColor:[NSColor colorWithSRGBRed:0.941 green:0.702 blue:0.082 alpha:1]];
  [gradient drawInRect:self.bounds angle:270];
}
@end
@interface AppDelegate : NSObject <NSApplicationDelegate, NSTableViewDataSource, NSTableViewDelegate, NSWindowDelegate> {
  std::unique_ptr<kz::Engine> engine;
  std::vector<kz::Entry> entries;
  kz::fs::path archive;
  BOOL busy;
}
@property NSWindow* window;
@property NSTableView* table;
@property NSTextField* status;
@property NSTextField* location;
@property NSSecureTextField* password;
@property NSTextField* split;
@property NSPopUpButton* splitPreset;
@property NSView* home;
@property NSPopUpButton* format;
@property NSButton* hideNames;
@property NSMutableArray<NSControl*>* controls;
@end
@implementation AppDelegate
- (NSTextField*)label:(NSString*)text frame:(NSRect)rect {
  NSTextField* field = [NSTextField labelWithString:text]; field.frame=rect; [self.window.contentView addSubview:field]; return field;
}
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  (void)notification;
  self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,1040,680) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable|NSWindowStyleMaskMiniaturizable backing:NSBackingStoreBuffered defer:NO];
  self.window.title=@"어린이zip"; self.window.minSize=NSMakeSize(1000,620); self.window.delegate=self;
  self.controls=[NSMutableArray array];
  NSImageView* logo=[[NSImageView alloc] initWithFrame:NSMakeRect(20,606,48,48)]; logo.image=NSApp.applicationIconImage; [self.window.contentView addSubview:logo];
  [self label:@"어린이zip   |   Native Preview 0.5.1" frame:NSMakeRect(80,632,800,24)];
  [self label:@"ZIP · 7Z · TAR / RAR 읽기 / ALZ · EGG 읽기 / ZPAQ" frame:NSMakeRect(80,606,850,24)];
  NSArray* titles=@[@"열기",@"파일 압축",@"폴더 압축",@"압축 풀기",@"검사",@"엔진 연결",@"취소",@"다시 읽기"];
  for(NSInteger i=0;i<8;i++) {
    NSButton* button=[NSButton buttonWithTitle:titles[i] target:self action:@selector(action:)];
    button.tag=i; button.frame=NSMakeRect(16+i*120,550,114,34); [self.window.contentView addSubview:button]; [self.controls addObject:button];
  }
  [self label:@"형식" frame:NSMakeRect(20,510,36,24)];
  self.format=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(60,508,84,28) pullsDown:NO]; [self.format addItemsWithTitles:@[@"zip",@"7z",@"tar",@"zpaq",@"gzip",@"bzip2",@"xz",@"wim",@"iso"]];
  [self.window.contentView addSubview:self.format];
  [self label:@"비밀번호" frame:NSMakeRect(164,510,70,24)];
  self.password=[[NSSecureTextField alloc] initWithFrame:NSMakeRect(235,508,180,28)]; [self.window.contentView addSubview:self.password];
  [self label:@"분할" frame:NSMakeRect(435,510,40,24)];
  self.splitPreset=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(475,508,210,28) pullsDown:NO];
  for(const auto& preset:kz::splitPresets)[self.splitPreset addItemWithTitle:S(preset.label)];
  self.splitPreset.target=self;self.splitPreset.action=@selector(optionsChanged:);
  [self.window.contentView addSubview:self.splitPreset];
  self.format.target=self;self.format.action=@selector(optionsChanged:);
  self.split=[[NSTextField alloc] initWithFrame:NSMakeRect(690,508,100,28)]; self.split.placeholderString=@"예: 100m"; [self.window.contentView addSubview:self.split];
  self.hideNames=[NSButton checkboxWithTitle:@"파일명 암호화 (7Z)" target:nil action:nil]; self.hideNames.frame=NSMakeRect(800,508,220,28); [self.window.contentView addSubview:self.hideNames];
  [self.controls addObjectsFromArray:@[self.format,self.password,self.split,self.splitPreset,self.hideNames]];
  self.location=[self label:@"압축파일을 열거나 새로 압축하세요. 결과는 지정한 위치에 저장됩니다." frame:NSMakeRect(20,468,990,28)];
  NSScrollView* scroll=[[NSScrollView alloc] initWithFrame:NSMakeRect(20,65,1000,390)]; scroll.hasVerticalScroller=YES; scroll.hasHorizontalScroller=YES;
  self.table=[[NSTableView alloc] initWithFrame:scroll.bounds]; self.table.delegate=self; self.table.dataSource=self;
  NSArray* names=@[@"이름",@"원본 크기 (B)",@"압축 크기 (B)",@"수정 날짜",@"암호"];
  for(NSInteger i=0;i<5;i++) { NSTableColumn* column=[[NSTableColumn alloc] initWithIdentifier:[NSString stringWithFormat:@"%ld",(long)i]]; column.title=names[i]; column.width=i==0?380:(i==3?190:130); [self.table addTableColumn:column]; }
  scroll.documentView=self.table; [self.window.contentView addSubview:scroll];
  self.status=[self label:@"준비 · 공식 7zz 실행 파일을 연결하세요." frame:NSMakeRect(20,12,1000,42)];
  for (const auto& candidate : {kz::fs::path("/opt/homebrew/bin/7zz"),kz::fs::path("/usr/local/bin/7zz")})
    if(kz::fs::exists(candidate)){engine=std::make_unique<kz::Engine>(candidate); self.status.stringValue=@"준비 · 7-Zip 연결됨"; break;}
  self.home=[[YellowHomeView alloc] initWithFrame:NSMakeRect(0,60,1040,620)];
  self.home.autoresizingMask=NSViewWidthSizable|NSViewHeightSizable;
  [self.window.contentView addSubview:self.home];
  NSImageView* homeLogo=[[NSImageView alloc] initWithFrame:NSMakeRect(20,536,60,60)];
  homeLogo.image=NSApp.applicationIconImage;homeLogo.autoresizingMask=NSViewMinYMargin;[self.home addSubview:homeLogo];
  for(NSInteger i=0;i<2;++i){
    NSButton* button=[NSButton buttonWithTitle:i==0?@"압축 파일 열기":@"새로 압축하기" target:self action:@selector(action:)];
    button.tag=i;button.frame=NSMakeRect(358+i*164,245,160,130);
    button.bezelStyle=NSBezelStyleRegularSquare;
    button.autoresizingMask=NSViewMinXMargin|NSViewMaxXMargin|NSViewMinYMargin|NSViewMaxYMargin;
    [self.home addSubview:button];[self.controls addObject:button];
  }
  NSMenuItem* viewItem=[[NSMenuItem alloc] initWithTitle:@"보기" action:nil keyEquivalent:@""];
  NSMenu* viewMenu=[[NSMenu alloc] initWithTitle:@"보기"];
  NSMenuItem* homeItem=[[NSMenuItem alloc] initWithTitle:@"시작 화면" action:@selector(showHome:) keyEquivalent:@"h"];
  homeItem.target=self;[viewMenu addItem:homeItem];viewItem.submenu=viewMenu;[NSApp.mainMenu addItem:viewItem];
  [self optionsChanged:nil];
  [self.window center]; [self.window makeKeyAndOrderFront:nil]; [NSApp activateIgnoringOtherApps:YES];
}
- (void)showHome:(id)sender { (void)sender; if(!busy)self.home.hidden=!self.home.hidden; }
- (void)optionsChanged:(id)sender {
  (void)sender;
  BOOL tar=self.format.indexOfSelectedItem==2||self.format.indexOfSelectedItem>=4,seven=self.format.indexOfSelectedItem==1;
  self.splitPreset.enabled=!busy&&!tar&&self.format.indexOfSelectedItem!=3;
  self.split.enabled=!busy&&!tar&&self.format.indexOfSelectedItem!=3&&self.splitPreset.indexOfSelectedItem==(NSInteger)kz::customSplitIndex;
  self.hideNames.enabled=!busy&&seven;if(!seven)self.hideNames.state=NSControlStateValueOff;
}
- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView { (void)tableView; return entries.size(); }
- (id)tableView:(NSTableView*)view objectValueForTableColumn:(NSTableColumn*)column row:(NSInteger)row {
  (void)view; const auto& e=entries.at(row);
  switch(column.identifier.integerValue) { case 0:return S(e.name); case 1:return @(e.size); case 2:return @(e.packed); case 3:return S(e.modified); default:return e.encrypted?@"암호":@""; }
}
- (std::vector<kz::fs::path>)chooseFolder:(BOOL)folder multiple:(BOOL)multiple {
  NSOpenPanel* panel=[NSOpenPanel openPanel]; panel.canChooseFiles=!folder; panel.canChooseDirectories=folder; panel.allowsMultipleSelection=multiple;
  std::vector<kz::fs::path> paths;
  if([panel runModal]==NSModalResponseOK) for(NSURL* url in panel.URLs) paths.emplace_back(url.fileSystemRepresentation);
  return paths;
}
- (void)run:(std::function<std::string()>)operation {
  if(busy) return;
  if(!engine) { self.status.stringValue=@"엔진 연결에서 공식 7zz를 선택하세요."; return; }
  busy=YES; engine->resetCancel(); for(NSControl* control in self.controls) control.enabled=NO;
  self.controls[6].enabled=YES; self.status.stringValue=@"작업 중…";
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0), ^{
    std::string message; try { message=operation(); } catch(const std::exception& e) { message=e.what(); }
    NSString* result=S(message);
    dispatch_async(dispatch_get_main_queue(), ^{ busy=NO; for(NSControl* control in self.controls) control.enabled=YES; self.status.stringValue=result; [self optionsChanged:nil]; });
  });
}
- (void)reloadArchive {
  if(archive.empty())return;
  self.home.hidden=YES;
  const auto file=archive; const std::string secret=self.password.stringValue.UTF8String;
  [self run:[self,file,secret] {
    auto data=std::make_shared<std::vector<kz::Entry>>(engine->list(file,secret));
    dispatch_async(dispatch_get_main_queue(), ^{ entries=*data; [self.table reloadData]; });
    return std::string("목록 읽기 완료");
  }];
}
- (void)action:(NSButton*)sender {
  NSInteger action=sender.tag;
  if(action==6) { if(engine) engine->cancel(); return; }
  if(busy)return;
  if(action==5) { auto files=[self chooseFolder:NO multiple:NO]; if(!files.empty()){engine=std::make_unique<kz::Engine>(files[0]); self.status.stringValue=@"7-Zip 엔진 연결됨";} return; }
  if(action==0) {
    auto files=[self chooseFolder:NO multiple:NO]; if(files.empty())return;
    if([S(files[0].extension().u8string()).lowercaseString isEqualToString:@".ezc"]){
      NSAlert* prompt=[NSAlert new];prompt.messageText=@"EasyCrypt 파일 해제";prompt.informativeText=@"비밀번호를 입력하세요. 원본 폴더에 해제합니다.";
      NSSecureTextField* field=[[NSSecureTextField alloc] initWithFrame:NSMakeRect(0,0,300,24)];prompt.accessoryView=field;
      [prompt addButtonWithTitle:@"해제"];[prompt addButtonWithTitle:@"취소"];
      if([prompt runModal]!=NSAlertFirstButtonReturn)return;
      std::string secret=field.stringValue.UTF8String;field.stringValue=@"";
      const auto input=files[0];[self run:[self,input,secret]{return "EasyCrypt 해제 완료: "+engine->decryptEasyCrypt(input,kz::fs::absolute(input).parent_path(),secret).u8string();}];return;
    }
    archive=files[0]; entries.clear(); [self.table reloadData]; self.location.stringValue=S(archive.u8string()); [self reloadArchive]; return;
  }
  if(action==7) { [self reloadArchive]; return; }
  if(action==1 || action==2) {
    NSAlert* options=[NSAlert new];options.messageText=@"새로 압축하기";options.informativeText=@"압축 형식과 분할 크기를 선택하세요.";
    [options addButtonWithTitle:@"파일 선택"];[options addButtonWithTitle:@"취소"];
    NSView* form=[[NSView alloc] initWithFrame:NSMakeRect(0,0,420,210)];
    NSArray* labels=@[@"압축 형식",@"분할 압축",@"직접 입력",@"비밀번호"];
    for(NSInteger i=0;i<4;++i){NSTextField* label=[NSTextField labelWithString:labels[i]];label.frame=NSMakeRect(0,180-i*38,85,22);[form addSubview:label];}
    NSPopUpButton* format=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(90,178,320,28) pullsDown:NO];[format addItemsWithTitles:@[@"zip",@"7z",@"tar",@"zpaq",@"gzip",@"bzip2",@"xz",@"wim",@"iso"]];
    [format selectItemAtIndex:self.format.indexOfSelectedItem];[form addSubview:format];
    NSPopUpButton* presets=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(90,140,320,28) pullsDown:NO];
    for(const auto& preset:kz::splitPresets)[presets addItemWithTitle:S(preset.label)];
    [presets selectItemAtIndex:self.splitPreset.indexOfSelectedItem];[form addSubview:presets];
    NSTextField* custom=[[NSTextField alloc] initWithFrame:NSMakeRect(90,104,320,24)];custom.placeholderString=@"직접 입력 선택 시 사용: 100m, 2g";custom.stringValue=self.split.stringValue;[form addSubview:custom];
    NSSecureTextField* secretField=[[NSSecureTextField alloc] initWithFrame:NSMakeRect(90,66,320,24)];[form addSubview:secretField];
    NSButton* encryptNames=[NSButton checkboxWithTitle:@"파일명 암호화 (7Z)" target:nil action:nil];encryptNames.frame=NSMakeRect(90,25,320,24);[form addSubview:encryptNames];
    options.accessoryView=form;
    if([options runModal]!=NSAlertFirstButtonReturn)return;
    [self.format selectItemAtIndex:format.indexOfSelectedItem];[self.splitPreset selectItemAtIndex:presets.indexOfSelectedItem];
    self.split.stringValue=custom.stringValue;self.password.stringValue=secretField.stringValue;secretField.stringValue=@"";
    self.hideNames.state=encryptNames.state;[self optionsChanged:nil];self.home.hidden=YES;
    auto inputs=[self chooseFolder:action==2 multiple:action==1]; if(inputs.empty())return;
    kz::Options o; o.format=self.format.titleOfSelectedItem.UTF8String; o.password=self.password.stringValue.UTF8String; try {
      if(o.format=="zip"||o.format=="7z")o.split=kz::splitArgument(static_cast<size_t>(self.splitPreset.indexOfSelectedItem),self.split.stringValue.UTF8String);
      if(o.format=="tar"||o.format=="gzip"||o.format=="bzip2"||o.format=="xz"||o.format=="wim"||o.format=="iso")o.password.clear();
      o.hideNames=o.format=="7z"&&self.hideNames.state==NSControlStateValueOn;
      kz::createArgs(o,"validation",inputs);
    }catch(const std::exception& e){self.status.stringValue=S(e.what());return;}
    NSSavePanel* panel=[NSSavePanel savePanel]; auto suggested=kz::suggestedArchive(inputs,o.format);panel.nameFieldStringValue=S(suggested.filename().u8string());panel.directoryURL=[NSURL fileURLWithPath:S(suggested.parent_path().u8string()) isDirectory:YES];
    if([panel runModal]!=NSModalResponseOK)return;
    kz::fs::path output(panel.URL.fileSystemRepresentation); self.password.stringValue=@"";
    [self run:[self,o,output,inputs] {return "압축 완료: "+engine->create(o,output,inputs).u8string();}]; return;
  }
  if(archive.empty()){self.status.stringValue=@"압축파일을 먼저 여세요.";return;}
  auto input=archive; std::string secret=self.password.stringValue.UTF8String;
  if(action==3) {auto folders=[self chooseFolder:YES multiple:NO]; if(folders.empty())return; auto parent=folders[0]; [self run:[self,input,parent,secret]{return "해제 완료: "+engine->extract(input,parent,secret).u8string();}];}
  if(action==4) [self run:[self,input,secret]{engine->test(input,secret);return std::string("압축파일 검사 완료");}];
}
- (BOOL)windowShouldClose:(NSWindow*)sender { (void)sender; if(busy){self.status.stringValue=@"작업을 취소한 뒤 닫아 주세요.";return NO;}return YES; }
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender { (void)sender; return busy?NSTerminateCancel:NSTerminateNow; }
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender { (void)sender; return YES; }
@end
int main() {
  @autoreleasepool {
    NSApplication* app=[NSApplication sharedApplication]; [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    NSMenu* menu=[NSMenu new]; NSMenuItem* item=[NSMenuItem new]; [menu addItem:item];
    NSMenu* appMenu=[NSMenu new]; [appMenu addItemWithTitle:@"Quit EoriniZip" action:@selector(terminate:) keyEquivalent:@"q"]; item.submenu=appMenu; app.mainMenu=menu;
    AppDelegate* delegate=[AppDelegate new]; app.delegate=delegate; [app run];
  } return 0;
}
