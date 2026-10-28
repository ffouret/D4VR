#ifndef D4VR_TYPED_COPY_SOURCE_MOCK_TYPES_H
#define D4VR_TYPED_COPY_SOURCE_MOCK_TYPES_H
/* CPU test types ONLY, not vendor headers. Real-D3D mode checks SDK data types
 * and source pointer-return methods, with an opaque Device15 surrogate. */
#include <stdint.h>
#if defined(D4VR_TC_REAL_D3D_TYPES)
#define CINTERFACE
#define COBJMACROS
#include <d3d12.h>
typedef struct d4vr_mock_device15 d3d12_device_iface;
typedef ID3D12Resource2 d3d12_resource_iface;
#else
#define STDMETHODCALLTYPE
 typedef uint32_t UINT;
 typedef uint32_t ULONG;
 typedef int32_t HRESULT;
 typedef struct {uint64_t ptr;} D3D12_CPU_DESCRIPTOR_HANDLE;
 typedef struct {uint64_t ptr;} D3D12_GPU_DESCRIPTOR_HANDLE;
 typedef uint32_t D3D12_DESCRIPTOR_HEAP_TYPE;
 typedef uint32_t D3D12_HEAP_FLAGS;
 typedef uint32_t D3D12_RESOURCE_STATES;
 typedef struct {uint32_t Data1;uint16_t Data2,Data3;uint8_t Data4[8];} GUID;
 typedef const GUID *REFIID;
 static const GUID IID_ID3D12Resource={0x696442be,0xa72e,0x4059,{0xbc,0x79,0x5b,0x5c,0x98,0x04,0x0f,0xad}};
 static const GUID IID_ID3D12DescriptorHeap={0x8efb471d,0x616c,0x4f49,{0x90,0xf7,0x12,0x7b,0xb7,0x63,0xfa,0x51}};
 typedef struct {UINT Type,NumDescriptors,Flags,NodeMask;} D3D12_DESCRIPTOR_HEAP_DESC;
 typedef struct {UINT Type,CPUPageProperty,MemoryPoolPreference,CreationNodeMask,VisibleNodeMask;} D3D12_HEAP_PROPERTIES;
 typedef struct {UINT Dimension;uint64_t Alignment,Width;UINT Height;uint16_t DepthOrArraySize,MipLevels;UINT Format;struct {UINT Count,Quality;} SampleDesc;UINT Layout,Flags;} D3D12_RESOURCE_DESC;
 typedef struct {UINT Format;float Color[4];} D3D12_CLEAR_VALUE;
 typedef struct {UINT Format,ViewDimension,Shader4ComponentMapping;union {struct {UINT MostDetailedMip,MipLevels,PlaneSlice;float ResourceMinLODClamp;} Texture2D;uint64_t ignored[4];};} D3D12_SHADER_RESOURCE_VIEW_DESC;
 typedef struct d4vr_mock_device15 d3d12_device_iface;
 typedef struct d4vr_mock_resource ID3D12Resource;
 typedef struct d4vr_mock_resource2 d3d12_resource_iface;
 typedef struct d4vr_mock_heap ID3D12DescriptorHeap;
#endif

_Static_assert(sizeof(GUID)==16,"typed GUID is exactly 16 bytes");
_Static_assert(sizeof(UINT)==4 && sizeof(ULONG)==4 && sizeof(HRESULT)==4,"Windows scalar widths");
_Static_assert(sizeof(float)==4,"D3D FLOAT bits");
_Static_assert(sizeof(D3D12_CPU_DESCRIPTOR_HANDLE)==8 && sizeof(D3D12_GPU_DESCRIPTOR_HANDLE)==8,"x64 handle structures");
#endif
