#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <sys/stat.h>
#include "../vulkan/descriptor_query_transport.hpp"

namespace {
struct Fake {void* dispatch;};
int instanceKey=0,deviceKey=0;
Fake fakeInstance{&instanceKey},fakeDevice{&deviceKey};
unsigned instanceCalls=0,deviceCalls=0,queryCalls=0,instanceDestroys=0,deviceDestroys=0;
uint64_t assertions=0;unsigned cases=0;
VkResult instanceStatus=VK_SUCCESS,deviceStatus=VK_SUCCESS;
const VkDescriptorGetInfoEXT* expectedInfo=nullptr;void* expectedOutput=nullptr;

void check(bool value,const char* what){++assertions;if(!value){std::fprintf(stderr,"mock check failed: %s\n",what);std::exit(3);}}
VKAPI_ATTR VkResult VKAPI_CALL createInstance(const VkInstanceCreateInfo* info,const VkAllocationCallbacks* allocation,VkInstance* out){
 check(info && info->pApplicationInfo && info->pApplicationInfo->apiVersion==VK_API_VERSION_1_3,"original instance info");check(!allocation,"original allocator");
 ++instanceCalls;*out=reinterpret_cast<VkInstance>(&fakeInstance);return instanceStatus;
}
VKAPI_ATTR void VKAPI_CALL destroyInstance(VkInstance instance,const VkAllocationCallbacks*){check(instance==reinterpret_cast<VkInstance>(&fakeInstance),"original instance");++instanceDestroys;}
VKAPI_ATTR VkResult VKAPI_CALL createDevice(VkPhysicalDevice physical,const VkDeviceCreateInfo*,const VkAllocationCallbacks*,VkDevice* out){
 check(physical==reinterpret_cast<VkPhysicalDevice>(&fakeInstance),"original physical");++deviceCalls;*out=reinterpret_cast<VkDevice>(&fakeDevice);return deviceStatus;
}
VKAPI_ATTR void VKAPI_CALL destroyDevice(VkDevice device,const VkAllocationCallbacks*){check(device==reinterpret_cast<VkDevice>(&fakeDevice),"original device");++deviceDestroys;}
VKAPI_ATTR void VKAPI_CALL query(VkDevice device,const VkDescriptorGetInfoEXT* info,size_t bytes,void* output){
 check(device==reinterpret_cast<VkDevice>(&fakeDevice),"original query device");check(info && info->type==VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && bytes==4,"original query arguments");
 check(info==expectedInfo && output==expectedOutput,"exact original query pointers");
 ++queryCalls;std::memset(output,0x5a,bytes);
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL deviceProc(VkDevice,const char* name){
 if(!std::strcmp(name,"vkGetDescriptorEXT"))return reinterpret_cast<PFN_vkVoidFunction>(query);
 if(!std::strcmp(name,"vkDestroyDevice"))return reinterpret_cast<PFN_vkVoidFunction>(destroyDevice);
 return nullptr;
}
VKAPI_ATTR VkResult VKAPI_CALL enumerate(VkInstance h,uint32_t* count,VkPhysicalDevice* output){
 check(h==reinterpret_cast<VkInstance>(&fakeInstance),"enumeration instance exact");check(count,"count pointer");
 if(output){check(*count>=1,"enumeration capacity");output[0]=reinterpret_cast<VkPhysicalDevice>(&fakeInstance);}*count=1;return VK_SUCCESS;
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL instanceProc(VkInstance,const char* name){
 if(!std::strcmp(name,"vkEnumeratePhysicalDevices"))return reinterpret_cast<PFN_vkVoidFunction>(enumerate);
 if(!std::strcmp(name,"vkCreateInstance"))return reinterpret_cast<PFN_vkVoidFunction>(createInstance);
 if(!std::strcmp(name,"vkDestroyInstance"))return reinterpret_cast<PFN_vkVoidFunction>(destroyInstance);
 if(!std::strcmp(name,"vkCreateDevice"))return reinterpret_cast<PFN_vkVoidFunction>(createDevice);
 if(!std::strcmp(name,"vkGetDescriptorEXT"))return reinterpret_cast<PFN_vkVoidFunction>(query);
 if(!std::strcmp(name,"vkDestroyDevice"))return reinterpret_cast<PFN_vkVoidFunction>(destroyDevice);
 return nullptr;
}
using Negotiate=VkResult(VKAPI_PTR*)(VkNegotiateLayerInterface*);
void exercise(void* library,bool withQuery){
 auto priorQueries=queryCalls;
 auto negotiate=reinterpret_cast<Negotiate>(dlsym(library,"vkNegotiateLoaderLayerInterfaceVersion"));check(negotiate,"negotiation export");
 VkNegotiateLayerInterface iface{};iface.sType=LAYER_NEGOTIATE_INTERFACE_STRUCT;iface.loaderLayerInterfaceVersion=2;check(negotiate(&iface)==VK_SUCCESS,"negotiation");
 VkLayerInstanceLink next{};next.pfnNextGetInstanceProcAddr=instanceProc;
 VkLayerInstanceCreateInfo link{};link.sType=VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO;link.function=VK_LAYER_LINK_INFO;link.u.pLayerInfo=&next;
 VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="owned CPU lifecycle mock";app.apiVersion=VK_API_VERSION_1_3;
 VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};info.pNext=&link;info.pApplicationInfo=&app;
 auto create=reinterpret_cast<PFN_vkCreateInstance>(iface.pfnGetInstanceProcAddr(VK_NULL_HANDLE,"vkCreateInstance"));VkInstance instance{};
 check(create && create(&info,nullptr,&instance)==VK_SUCCESS,"instance creation");check(link.u.pLayerInfo==nullptr,"required instance loader advancement");
 if(!withQuery){check(queryCalls==priorQueries,"no injected query during enumeration");return;}
 uint32_t count=1;VkPhysicalDevice found{};
 auto enumerateP=reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(iface.pfnGetInstanceProcAddr(instance,"vkEnumeratePhysicalDevices"));
 check(enumerateP&&enumerateP(instance,&count,&found)==VK_SUCCESS,"original physical enumeration");
 check(found==reinterpret_cast<VkPhysicalDevice>(&fakeInstance),"physical result");
 VkLayerDeviceLink deviceNext{};deviceNext.pfnNextGetInstanceProcAddr=instanceProc;deviceNext.pfnNextGetDeviceProcAddr=deviceProc;
 VkLayerDeviceCreateInfo deviceLink{};deviceLink.sType=VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO;deviceLink.function=VK_LAYER_LINK_INFO;deviceLink.u.pLayerInfo=&deviceNext;
 VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};deviceInfo.pNext=&deviceLink;
 auto createD=reinterpret_cast<PFN_vkCreateDevice>(iface.pfnGetInstanceProcAddr(instance,"vkCreateDevice"));VkDevice device{};
 check(createD && createD(reinterpret_cast<VkPhysicalDevice>(instance),&deviceInfo,nullptr,&device)==VK_SUCCESS,"device creation");
 check(deviceLink.u.pLayerInfo==nullptr,"required device loader advancement");
 VkDescriptorImageInfo image{};image.imageView=reinterpret_cast<VkImageView>(uintptr_t(0x1234));image.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
 VkDescriptorGetInfoEXT get{VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT};get.type=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;get.data.pSampledImage=&image;
 unsigned char output[4]{};expectedInfo=&get;expectedOutput=output;auto proc=reinterpret_cast<PFN_vkGetDescriptorEXT>(iface.pfnGetDeviceProcAddr(device,"vkGetDescriptorEXT"));check(proc,"query proc");proc(device,&get,sizeof(output),output);
 for(auto value:output)check(value==0x5a,"original output exact");
 auto destroyD=reinterpret_cast<PFN_vkDestroyDevice>(iface.pfnGetDeviceProcAddr(device,"vkDestroyDevice"));check(destroyD,"destroy device proc");destroyD(device,nullptr);
 auto destroyI=reinterpret_cast<PFN_vkDestroyInstance>(iface.pfnGetInstanceProcAddr(instance,"vkDestroyInstance"));check(destroyI,"destroy instance proc");destroyI(instance,nullptr);check(queryCalls==priorQueries+1,"original query exactly once");
}
void* load(const char* path,bool isolated=false){
 void* result=isolated?dlmopen(LM_ID_NEWLM,path,RTLD_NOW|RTLD_LOCAL):dlopen(path,RTLD_NOW|RTLD_LOCAL);
 if(!result){std::fprintf(stderr,"dlopen: %s\n",dlerror());std::exit(4);}return result;
}
}

namespace fs=std::filesystem;
std::string read(const fs::path& path){std::ifstream input(path,std::ios::binary);check(bool(input),"open test artifact");return {std::istreambuf_iterator<char>(input),{}};}
void put(const fs::path& path,const std::string& text){std::ofstream output(path,std::ios::binary);output<<text;check(bool(output),"write test artifact");}
void directory(const fs::path& path){check(fs::create_directory(path),"fresh directory");fs::permissions(path,fs::perms::owner_all);}
std::size_t occurrences(const std::string& text,const std::string& needle){std::size_t n=0,p=0;while((p=text.find(needle,p))!=std::string::npos){++n;p+=needle.size();}return n;}
int child(const char* path,const char* mode){
 if(!std::strcmp(mode,"enumeration")||!std::strcmp(mode,"query")){
  auto library=load(path);exercise(library,!std::strcmp(mode,"query"));check(dlclose(library)==0,"dlclose");
 }else if(!std::strcmp(mode,"cycle")||!std::strcmp(mode,"reuse")){
  auto first=load(path);exercise(first,!std::strcmp(mode,"reuse"));check(dlclose(first)==0,"first dlclose");
  auto second=load(path);exercise(second,true);check(dlclose(second)==0,"second dlclose");
 }else if(!std::strcmp(mode,"realms")){
  auto first=load(path,true);exercise(first,false);auto second=load(path,true);exercise(second,true);
  check(dlclose(second)==0,"second namespace dlclose");check(dlclose(first)==0,"first namespace dlclose");
 }else if(!std::strcmp(mode,"quota")){
  auto library=load(path);
  for(unsigned c=1;c<=8;++c)for(unsigned n=0;n<5;++n){std::ofstream phase(std::getenv("D4VR_QUERY_PHASE_FILE"));phase<<"D4VR_QUERY_PHASE_1 "<<c<<" 1\n";phase.close();exercise(library,true);}
  check(dlclose(library)==0,"quota dlclose");
 }else if(!std::strcmp(mode,"fork_runtime")){
  auto library=load(path);exercise(library,true);auto pid=fork();check(pid>=0,"fork live Runtime");
  if(pid==0){auto before=queryCalls;exercise(library,true);check(queryCalls==before+1,"forked original query forwards");check(dlclose(library)==0,"forked DSO close");_exit(0);}
  int status=0;check(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0,"forked Runtime driver exact");check(dlclose(library)==0,"parent DSO close");
 }else if(!std::strcmp(mode,"live_reuse")){
  auto library=load(path);for(unsigned i=0;i<80;++i)exercise(library,true);check(dlclose(library)==0,"80 churn dlclose");
 }else check(false,"known child mode");
 std::printf("{\"child\":true,\"mode\":\"%s\",\"original_queries\":%u,\"assertions\":%llu,\"GPU_calls\":0}\n",mode,queryCalls,static_cast<unsigned long long>(assertions));
 return 0;
}
pid_t launch(const fs::path& self,const fs::path& library,const fs::path& folder,const char* mode,const char* enabled="1"){
 auto pid=fork();check(pid>=0,"fork CPU mock");
 if(pid==0){
  auto canonical=fs::canonical(folder.parent_path())/folder.filename();setenv("D4VR_QUERY_OBSERVER_DIRECTORY",canonical.c_str(),1);setenv("D4VR_TRANSLATED_RUN_ID","0123456789abcdef0123456789abcdef",1);
  setenv("D4VR_DESCRIPTOR_QUERY_OBSERVER",enabled,1);auto phase=folder/"phase";setenv("D4VR_QUERY_PHASE_FILE",phase.c_str(),1);
  execl(self.c_str(),self.c_str(),"--child",library.c_str(),mode,nullptr);_exit(127);
 }
 return pid;
}
void waitFor(pid_t pid){int status=0;check(waitpid(pid,&status,0)==pid,"wait CPU mock");check(WIFEXITED(status)&&WEXITSTATUS(status)==0,"CPU child successful");}
void run(const fs::path& self,const fs::path& library,const fs::path& folder,const char* mode,const char* enabled="1"){waitFor(launch(self,library,folder,mode,enabled));}
void setup(const fs::path& path){directory(path);put(path/"phase","D4VR_QUERY_PHASE_1 1 1\n");}
void has(const std::string& text,const std::string& field){check(text.find(field)!=std::string::npos,field.c_str());}
void tests(const fs::path& self,const fs::path& library,const fs::path& root,bool namespaces){
 // Independent PIDs and simultaneous namespace lifetimes get separate claims.
 {++cases;auto path=root/"two-pids";setup(path);auto first=launch(self,library,path,"enumeration");auto second=launch(self,library,path,"query");waitFor(first);waitFor(second);
  check(fs::exists(path/"slot-0.claim")&&fs::exists(path/"slot-1.claim"),"two persistent claims");
  auto a=read(path/"slot-0.claim"),b=read(path/"slot-1.claim");check(a!=b,"distinct PID and slot identities");
  auto joined=read(path/"slot-0.jsonl")+read(path/"slot-1.jsonl");has(joined,"\"calls_seen\":1");has(joined,"\"calls_seen\":0");check(occurrences(joined,"\"event\":\"observer_header\"")==2,"two headers");}
 for(auto mode:{"cycle","realms"}){if(!namespaces&&!std::strcmp(mode,"realms"))continue;++cases;auto path=root/mode;setup(path);run(self,library,path,mode);
  check(fs::exists(path/"slot-1.claim"),"reload/namespace has second persistent claim");auto first=read(path/"slot-0.jsonl"),second=read(path/"slot-1.jsonl");
  has(first,"\"calls_seen\":0");has(second,"\"calls_seen\":1");has(second,"\"device_lifetime_known\":true");}
 // Exactly eight global claims; ninth still forwards and cannot evict any bytes.
 {++cases;auto path=root/"eight-claims";setup(path);std::vector<std::string> before;
  for(unsigned i=0;i<8;++i)run(self,library,path,"query");
  for(unsigned i=0;i<8;++i)before.push_back(read(path/("slot-"+std::to_string(i)+".jsonl")));
  run(self,library,path,"query");has(read(path/"overflow"),"D4VR_QUERY_SCOPE_OVERFLOW_2");
  check(!fs::exists(path/"slot-8.claim"),"no ninth claim");
  for(unsigned i=0;i<8;++i)check(before[i]==read(path/("slot-"+std::to_string(i)+".jsonl")),"no overwrite/eviction");}
 // Device/raw handle reuse inside one DSO has distinct immutable generations.
 {++cases;auto path=root/"live-reuse";setup(path);run(self,library,path,"live_reuse");auto text=read(path/"slot-0.jsonl");
  has(text,"\"device_generation\":1");has(text,"\"device_generation\":2");has(text,"\"lifecycle_dropped\":");has(text,"\"faulted\":true");
  check(occurrences(text,"\"event\":\"get_descriptor\"")<=4,"per-case quota4");has(text,"\"coverage_complete\":false");}
 {++cases;auto path=root/"global-quota";setup(path);std::size_t total=0;
  for(unsigned i=0;i<8;++i){run(self,library,path,"quota");auto text=read(path/("slot-"+std::to_string(i)+".jsonl"));
   check(occurrences(text,"\"event\":\"get_descriptor\"")==32,"scope event cap32");has(text,"\"events_by_case\":[0,4,4,4,4,4,4,4,4]");has(text,"\"dropped_events\":8");total+=32;}
  check(total==256,"global8x32cap256");}
 {++cases;auto path=root/"fork-runtime";setup(path);run(self,library,path,"fork_runtime");auto text=read(path/"slot-0.jsonl");has(text,"\"calls_seen\":1");check(!fs::exists(path/"slot-1.claim"),"child never retags inherited Runtime");}
 // OFF and invalid directories preserve actual original query forwarding.
 {++cases;auto path=root/"off";setup(path);run(self,library,path,"query","0");auto text=read(path/"slot-0.jsonl");has(text,"\"enabled\":false");has(text,"\"calls_seen\":0");check(occurrences(text,"\"event\":\"get_descriptor\"")==0,"off metadata query inert");}
 {++cases;auto path=root/"bad-mode";setup(path);fs::permissions(path,fs::perms::owner_all|fs::perms::group_read);run(self,library,path,"query");check(!fs::exists(path/"slot-0.claim"),"wrong permissions refuse transport only");}
 {++cases;auto target=root/"real-directory";setup(target);auto path=root/"symlink-directory";fs::create_directory_symlink(target,path);run(self,library,path,"query");check(!fs::exists(target/"slot-0.claim"),"nofollow directory");}
 // Immediate header survives without footer. Forked transport must never emit old PID.
 {++cases;auto path=root/"fork-guard";setup(path);d4vr::query_observer::ScopedTransport transport(fs::canonical(path).c_str(),"0123456789abcdef0123456789abcdef");
  check(transport.valid(),"claim success");check(transport.identity().hostPid==uint64_t(getpid()),"host PID identity");check(transport.identity().processStartCookie>0,"bounded proc start cookie");
  check(transport.writeHeader("header\nfooter\n"),"immediate header");check(read(path/"slot-0.jsonl")=="header\n","header before teardown");
  auto pid=fork();check(pid>=0,"fork inherited FD");if(pid==0){bool rejected=!transport.ownerMatches()&&!transport.valid();_exit(rejected?0:3);}waitFor(pid);
  check(read(path/"slot-0.jsonl")=="header\n","child did not retag/write inherited report");check(transport.finish("header\nfooter\n"),"parent footer retained");}
 // A preexisting report and symlink claim are never overwritten.
 {++cases;auto path=root/"existing-report";setup(path);put(path/"slot-0.jsonl","preserved");run(self,library,path,"query");check(read(path/"slot-0.jsonl")=="preserved","report O_EXCL");check(fs::exists(path/"slot-0.claim"),"failed output claim persists");}
}
int main(int argc,char** argv)try{
 if(argc==4&&!std::strcmp(argv[1],"--child"))return child(argv[2],argv[3]);
 fs::path library,root;bool namespaces=true;
 for(int i=1;i<argc;++i){const std::string key=argv[i];
  if(key=="--no-new-namespaces"){check(namespaces,"no repeated namespace option");namespaces=false;}
  else if(key=="--library"||key=="--report"){check(i+1<argc,"option value");auto& target=key=="--library"?library:root;check(target.empty(),"no repeated option");target=fs::absolute(argv[++i]);}
  else check(false,"Usage: tests --library ELF [--report NEW_DIRECTORY] [--no-new-namespaces]");
 }
 check(!library.empty(),"library required");fs::path self=fs::canonical("/proc/self/exe");
 if(root.empty())root=fs::temp_directory_path()/("d4vr-scope-tests-"+std::to_string(getpid()));
 directory(root);tests(self,library,root,namespaces);
 const auto summary="{\"descriptor_query_scope_passed\":true,\"cases\":"+std::to_string(cases)+",\"assertions\":"+std::to_string(assertions)+",\"new_namespace_test_executed\":"+(namespaces?"true":"false")+",\"namespace_cases_explicitly_skipped\":"+(namespaces?"0":"1")+",\"GPU_calls\":0}\n";
 put(root/"summary.json",summary);std::printf("%s",summary.c_str());
 return 0;
}catch(const std::exception& e){std::fprintf(stderr,"scope CPU test failed: %s\n",e.what());return 1;}
