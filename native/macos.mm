#import <Cocoa/Cocoa.h>
#include "core.hpp"
#include <memory>
#include <functional>
static NSString* S(const std::string& s) { return [[NSString alloc] initWithBytes:s.data() length:s.size() encoding:NSUTF8StringEncoding] ?: @"Invalid UTF-8"; }
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
  [self label:@"어린이zip   |   Native Preview 0.1" frame:NSMakeRect(80,632,800,24)];
  [self label:@"ZIP · 7Z · TAR / RAR 읽기 / ALZ · EGG · ZPAQ 개발 예정" frame:NSMakeRect(80,606,850,24)];
  NSArray* titles=@[@"열기",@"파일 압축",@"폴더 압축",@"압축 풀기",@"검사",@"엔진 연결",@"취소",@"다시 읽기"];
  for(NSInteger i=0;i<8;i++) {
    NSButton* button=[NSButton buttonWithTitle:titles[i] target:self action:@selector(action:)];
    button.tag=i; button.frame=NSMakeRect(16+i*120,550,114,34); [self.window.contentView addSubview:button]; [self.controls addObject:button];
  }
  [self label:@"형식" frame:NSMakeRect(20,510,36,24)];
  self.format=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(60,508,84,28) pullsDown:NO]; [self.format addItemsWithTitles:@[@"zip",@"7z",@"tar"]];
  [self.window.contentView addSubview:self.format];
  [self label:@"비밀번호" frame:NSMakeRect(164,510,70,24)];
  self.password=[[NSSecureTextField alloc] initWithFrame:NSMakeRect(235,508,180,28)]; [self.window.contentView addSubview:self.password];
  [self label:@"분할" frame:NSMakeRect(435,510,40,24)];
  self.split=[[NSTextField alloc] initWithFrame:NSMakeRect(480,508,110,28)]; self.split.placeholderString=@"예: 100m"; [self.window.contentView addSubview:self.split];
  self.hideNames=[NSButton checkboxWithTitle:@"파일명 암호화 (7Z)" target:nil action:nil]; self.hideNames.frame=NSMakeRect(610,508,220,28); [self.window.contentView addSubview:self.hideNames];
  [self.controls addObjectsFromArray:@[self.format,self.password,self.split,self.hideNames]];
  self.location=[self label:@"압축파일을 열거나 새로 압축하세요. 결과는 새 어린이zip 폴더에 저장됩니다." frame:NSMakeRect(20,468,990,28)];
  NSScrollView* scroll=[[NSScrollView alloc] initWithFrame:NSMakeRect(20,65,1000,390)]; scroll.hasVerticalScroller=YES; scroll.hasHorizontalScroller=YES;
  self.table=[[NSTableView alloc] initWithFrame:scroll.bounds]; self.table.delegate=self; self.table.dataSource=self;
  NSArray* names=@[@"이름",@"원본 크기 (B)",@"압축 크기 (B)",@"수정 날짜",@"암호"];
  for(NSInteger i=0;i<5;i++) { NSTableColumn* column=[[NSTableColumn alloc] initWithIdentifier:[NSString stringWithFormat:@"%ld",(long)i]]; column.title=names[i]; column.width=i==0?380:(i==3?190:130); [self.table addTableColumn:column]; }
  scroll.documentView=self.table; [self.window.contentView addSubview:scroll];
  self.status=[self label:@"준비 · 공식 7zz 실행 파일을 연결하세요." frame:NSMakeRect(20,12,1000,42)];
  for (const auto& candidate : {kz::fs::path("/opt/homebrew/bin/7zz"),kz::fs::path("/usr/local/bin/7zz")})
    if(kz::fs::exists(candidate)){engine=std::make_unique<kz::Engine>(candidate); self.status.stringValue=@"준비 · 7-Zip 연결됨"; break;}
  [self.window center]; [self.window makeKeyAndOrderFront:nil]; [NSApp activateIgnoringOtherApps:YES];
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
    dispatch_async(dispatch_get_main_queue(), ^{ busy=NO; for(NSControl* control in self.controls) control.enabled=YES; self.status.stringValue=result; });
  });
}
- (void)reloadArchive {
  if(archive.empty())return;
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
    auto files=[self chooseFolder:NO multiple:NO]; if(files.empty())return; archive=files[0]; entries.clear(); [self.table reloadData]; self.location.stringValue=S(archive.u8string()); [self reloadArchive]; return;
  }
  if(action==7) { [self reloadArchive]; return; }
  if(action==1 || action==2) {
    auto inputs=[self chooseFolder:action==2 multiple:action==1]; if(inputs.empty())return;
    kz::Options o; o.format=self.format.titleOfSelectedItem.UTF8String; o.password=self.password.stringValue.UTF8String; o.split=self.split.stringValue.UTF8String; o.hideNames=self.hideNames.state==NSControlStateValueOn;
    NSSavePanel* panel=[NSSavePanel savePanel]; panel.nameFieldStringValue=S("Archive."+o.format);
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
