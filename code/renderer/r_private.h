/*
Copyright (c) 2021-2022 Bjarke Damsgaard Eriksen. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    1. Redistributions of source code must retain the above
       copyright notice, this list of conditions and the
       following disclaimer.

    2. Redistributions in binary form must reproduce the above
       copyright notice, this list of conditions and the following
       disclaimer in the documentation and/or other materials
       provided with the distribution.

    3. Neither the name of the copyright holder nor the names of
       its contributors may be used to endorse or promote products
       derived from this software without specific prior written
       permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#include "r_public.h"

#define WIN32_LEAN_AND_MEAN // so that Windows.h includes a lot less garbage
#include "d3d11.h" // the Windows 7 SDK version is too old and missing stuff we want
#include "d3d11_3.h" // not available in the Windows 7 SDK, hence the local copy
#include <Windows.h>

#include "../shaders/deferred_options.hlsli"
#include "../shaders/material_flags.hlsli"

#define MAX_TEXTURES 256
#define MAX_VERTEXES 8 * 3
#define MAX_INDEXES 36 * 3

#define COM_RELEASE(p)    \
    do                    \
    {                     \
        if (p)            \
        {                 \
            p->Release(); \
            p = NULL;     \
        }                 \
    } while ((void)0, 0)
#define COM_RELEASE_ARRAY(a)                   \
    do                                         \
    {                                          \
        for (int i = 0; i < ARRAY_LEN(a); ++i) \
        {                                      \
            COM_RELEASE(a[i]);                 \
        }                                      \
    } while ((void)0, 0)

//
// vertex buffer attributes
//

// clang-format off
#define VB_LIST(V) \
V(Position, DXGI_FORMAT_R32G32B32_FLOAT, "POSITION", vec3_t) \
V(Normal, DXGI_FORMAT_R32G32B32_FLOAT, "NORMAL", vec3_t) \
V(Color, DXGI_FORMAT_R32G32B32_FLOAT, "COLOR", vec3_t) \
V(Tc, DXGI_FORMAT_R32G32_FLOAT, "TEXCOORD", vec2_t)
// clang-format on

struct VertexBufferId
{
    enum Type
    {
#define VB_ITEM(EnumId, Format, Name, Type) EnumId,
        VB_LIST(VB_ITEM)
#undef VB_ITEM
            Count
    };
};

extern DXGI_FORMAT vbFormats[VertexBufferId::Count + 1];
extern const char* vbNames[VertexBufferId::Count + 1];
extern size_t vbItemSizes[VertexBufferId::Count + 1];

// We use the vertex buffer for holding vertices for rendering.
struct VertexBuffer
{
    ID3D11Buffer* buffer;
    u32 itemSize;
    u32 capacity;
    u32 writeIndex;
    u32 readIndex;
    bool discard;
};

struct DrawBuffer
{
    VertexBuffer vertexBuffers[VertexBufferId::Count];
    VertexBuffer indexBuffer;
    u32 numIndexes;
};

//
// queries
//

// clang-format off
#define QUERY_LIST(Q) \
Q(Shadows, "shadows") \
Q(OpacityVoxelization, "opacity voxelization") \
Q(EmittanceVoxelization, "emittance voxelization") \
Q(VoxelizationFix, "voxelization fix") \
Q(OpacityMipMapping, "opacity mip-mapping") \
Q(EmittanceFormatFix, "emittance format fix") \
Q(EmittanceOpacityFix, "emittance opacity fix") \
Q(EmittanceBounce, "emittance bounce") \
Q(EmittanceMipMapping, "emittance mip-mapping") \
Q(HiZ, "hierarchical z") \
Q(GeometryPass, "geometry") \
Q(SSSOPass, "ssso") \
Q(SSSOFilter, "ssso filter") \
Q(DeferredShading, "deferred shading") \
Q(VoxelViz, "voxel visualization") \
Q(FullFrame, "full frame")
// clang-format on

struct QueryId
{
    enum Type
    {
#define QUERY_ITEM(EnumId, Name) EnumId,
        QUERY_LIST(QUERY_ITEM)
#undef QUERY_ITEM
            Count
    };
};

extern const char* queryNames[QueryId::Count + 1];

void BeginQuery(QueryId::Type queryId);
void EndQuery(QueryId::Type queryId);
void BeginFrameQueries();
void EndFrameQueries();

struct Query
{
    ID3D11Query* disjoint;
    ID3D11Query* begin;
    ID3D11Query* end;
    bool active;
};

struct FrameQuery
{
    const char* name;
    f32 medianUS;
    f32 minUS;
    f32 maxUS;
    f32 frameTimes[32];

    Query queries[8];
    u32 queryWriteIndex;
    u32 queryReadIndex;

    bool active;
};

//
// scoped regions
//

void BeginDebugRegion(const wchar_t* name);
void EndDebugRegion();

struct ScopedDebugRegion
{
    ScopedDebugRegion(const wchar_t* name)
    {
        BeginDebugRegion(name);
    }

    ~ScopedDebugRegion()
    {
        EndDebugRegion();
    }
};

#define DEBUG_REGION(Name) ScopedDebugRegion debugRegion(L##Name)

struct ScopedQueryRegion
{
    QueryId::Type _id;
    ScopedQueryRegion(QueryId::Type id)
    {
        _id = id;
        BeginQuery(id);
    }

    ~ScopedQueryRegion()
    {
        EndQuery(_id);
    }
};

#define QUERY_REGION(Id) ScopedQueryRegion queryRegion(Id)

//
// cached states
//

struct CachedRasterState
{
    D3D11_RASTERIZER_DESC desc;
    ID3D11RasterizerState* state;
};

struct CachedRasterState2
{
    D3D11_RASTERIZER_DESC2 desc;
    ID3D11RasterizerState2* state;
};

struct CachedBlendState
{
    D3D11_BLEND_DESC desc;
    ID3D11BlendState* state;
};

struct CachedSamplerState
{
    D3D11_SAMPLER_DESC desc;
    ID3D11SamplerState* state;
};

struct CachedDepthState
{
    D3D11_DEPTH_STENCIL_DESC desc;
    ID3D11DepthStencilState* state;
};

typedef DynamicArray<ID3D11DeviceChild*> ResourceArray;

struct DepthMap
{
    ID3D11Texture2D* map;
    ID3D11ShaderResourceView* fullSRV;
    ID3D11ShaderResourceView* SRVs[16];
    ID3D11UnorderedAccessView* UAVs[16];
};

struct Direct3D
{
    ID3D11Texture2D* backBufferTexture; // we do not own the texture, so we do not release it
    ID3D11RenderTargetView* backBufferRTView;

    ID3D11DepthStencilView* depthStencilView;
    ID3D11ShaderResourceView* depthSRV;
    ID3D11Texture2D* depthStencilTexture;
    ID3D11DepthStencilState* depthState;
    ID3D11DepthStencilState* postProcessDepthState; // no depth test/write
    ID3D11RasterizerState* rasterState;

    DepthMap depthMap;

    CachedDepthState depthStates[10];
    u32 numDepthStates;

    CachedSamplerState samplerStates[10];
    u32 numSamplerStates;

    CachedRasterState rasterStates[10];
    u32 numRasterStates;
    CachedRasterState2 rasterStates2[10];
    u32 numRasterStates2;
    CachedBlendState blendStates[10];
    u32 numBlendStates;
    ID3D11BlendState* blendStateDisabled;
    ID3D11BlendState* blendStateStandard;

    ResourceArray persistent;
    ResourceArray resDependent;

    FrameQuery frameQueries[QueryId::Count];

    VertexBufferId::Type vbIds[VertexBufferId::Count]; // After applying a pipleline the vertex buffers
    ID3D11Buffer* vbBuffers[VertexBufferId::Count]; // Get stored here, so we can use them when setting the
    UINT vbStrides[VertexBufferId::Count]; // Input layout.
    int vbCount;

    UINT swapInterval;

    u32 frameNumber;
};

struct Direct3DStatic
{
    ID3D11Device* device; // Used to create resources.
    ID3D11DeviceContext* context; // Used to generate rendering commands.
    IDXGISwapChain* swapChain; // Stores rendered data before presenting it to an output.

    ID3DUserDefinedAnnotation* annotation;

    // DIRECT3D11_3 Used for conservative razterization.
    ID3D11Device3* device3;
    ID3D11DeviceContext* context3;
};

ID3D11BlendState* CreateBlendState(const D3D11_BLEND_DESC* bDesc);
ID3D11RasterizerState2* CreateRasterizerState2(const D3D11_RASTERIZER_DESC2* rasterDesc);
ID3D11RasterizerState* CreateRasterizerState(const D3D11_RASTERIZER_DESC* rasterDesc);
ID3D11DepthStencilState* CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC* desc);

extern Direct3D d3d;
extern Direct3DStatic d3ds;

//
// shared asset data
//

// First eight slots in textures/textureViews are the default textures
struct Scene;
struct AssetsSharedData
{
    DrawBuffer drawBuffer;
    DrawBuffer drawBufferLowRes;

    ID3D11Texture2D* textures[MAX_TEXTURES];
    ID3D11ShaderResourceView* textureViews[MAX_TEXTURES];
    StaticHashMap<u32, Image, 25> textureMap;
    u32 numTextures; // also numViews (since textureViews[i] is a view of textures[i])

    Scene* currentMesh;
    u32 assetID;

    MemoryArena oldStrings;
    MemoryArena newStrings;
};

extern AssetsSharedData assetsShared;

void WriteBinaryMaterialToFile(Scene* scene, const char* filePath);
unsigned long HashTextureName(const char* str);

//
// shader buffer descriptions
//

#pragma pack(push, 1)
struct DeferredShadingLight
{
    vec4_t position;
    vec4_t direction;
    vec4_t color;
    vec4_t params;
    f32 lightView[16];
    f32 lightProj[16];
};

struct DeferredShadingPSData
{
    m4x4 invProjectionMatrix;

    vec4_t bounding_min;
    vec4_t bounding_max;
    vec4_t cameraPosition;
    vec4_t viewVector;

    // Constants
    f32 temp[4];

    f32 cst_opacityThreshold;
    f32 cst_traceStepScale;
    f32 cst_occlusionScale;
    f32 cst_coneRatio;
    f32 cst_maxMipLevel;
    f32 cst_mipBias;
    f32 cst_slopeBias;
    f32 cst_fixedBias;
    f32 cst_maxBias;
    f32 fdummy[3];

    u32 cst_options;
    u32 dummy[3];
};

struct ShaderMaterial
{
    vec4_t specular; // Ks (rgb) + Ns (a)
    u32 flags;
    u32 dummy[3];
};

struct MaterialShadingData
{
    ShaderMaterial materials[MAX_MATERIALS];
};

struct LightPSData
{
    DeferredShadingLight lights[MAX_LIGHTS];
    u32 lightCount;
    u32 dummy[3];
};

#pragma pack(pop)

struct GBufferId
{
    enum Type
    {
        Albedo,
        Normal,
        Material,
        Count
    };
};

struct GraphicsPipeline
{
    struct ShaderStage
    {
        ID3D11Buffer* buffers[12];
        ID3D11ShaderResourceView* srvs[12];
        ID3D11SamplerState* samplers[12];
        u32 numBuffers;
        u32 numSRVs;
        u32 numSamplers;
    };

    struct ShaderStageVS : public ShaderStage
    {
        ID3D11VertexShader* shader;
    };

    struct ShaderStageGS : public ShaderStage
    {
        ID3D11GeometryShader* shader;
    };

    struct ShaderStagePS : public ShaderStage
    {
        ID3D11PixelShader* shader;
    };

    struct
    {
        ID3D11Buffer* indexBuffer;
        ID3D11InputLayout* layout;
        D3D11_PRIMITIVE_TOPOLOGY topology;
        ID3D11Buffer* vertexBuffers[8];
        UINT strides[8];
        UINT offsets[8];
        u32 numVertexBuffers;
    } ia;

    ShaderStageVS vs;
    ShaderStageGS gs;

    struct
    {
        D3D11_VIEWPORT viewports[8];
        D3D11_RECT scissors[8];
        ID3D11RasterizerState* state;
        u32 numViewports;
        u32 numScissors;
    } rs;

    ShaderStagePS ps;

    struct
    {
        ID3D11BlendState* blendState;
        ID3D11DepthStencilState* depthState;
        ID3D11DepthStencilView* depthView;
        ID3D11RenderTargetView* rtvs[8];
        ID3D11UnorderedAccessView* uavs[8];
        u32 numRTVs;
        u32 numUAVs;
    } om;
};
extern bool graphicsPipelineValid; // cache invalidation

struct ComputePipeline
{
    ID3D11ComputeShader* shader;
    ID3D11Buffer* buffers[8];
    ID3D11ShaderResourceView* srvs[8];
    ID3D11UnorderedAccessView* uavs[8];
    ID3D11SamplerState* samplers[8];
    u32 numBuffers;
    u32 numSRVs;
    u32 numUAVs;
    u32 numSamplers;
};



//
// voxels
//

struct EmittanceMap
{
    ID3D11Texture3D* map;
    ID3D11ShaderResourceView* fullSRV;
    ID3D11ShaderResourceView* SRVs[16];
    ID3D11UnorderedAccessView* UAVs[16];

    ID3D11UnorderedAccessView* unormUAVs[16];
};

struct VoxelSharedData
{
    ID3D11Texture3D* opacityMap;
    ID3D11ShaderResourceView* opacityMapFullSRV;
    ID3D11ShaderResourceView* opacityMapSRVs[16];
    ID3D11UnorderedAccessView* opacityMapUAVs[16];

    EmittanceMap emittanceMaps[3];
    EmittanceMap emittanceHDR[3];
    u32 emittanceReadIndex;

    ID3D11Texture3D* normalMap;
    ID3D11ShaderResourceView* normalMapSRV;
    ID3D11UnorderedAccessView* normalMapUAV;

    uint3_t gridSize;
    u32 maxGapDistance;
    u32 numBounces;
    UINT emittanceNumBytes;
    UINT opacityNumBytes;
    UINT normalNumBytes;
};

extern VoxelSharedData voxelShared;

//
// helper functions
//

ID3D11ShaderResourceView* CreateShaderResourceView(ResourceArray* resources, ID3D11Resource* resource, const D3D11_SHADER_RESOURCE_VIEW_DESC* srvDesc, const char* name);
ID3D11UnorderedAccessView* CreateUnorderedAccessView(ResourceArray* resources, ID3D11Resource* resource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* uavDesc, const char* name);
ID3D11Texture3D* CreateTexture3D(ResourceArray* resources, const D3D11_TEXTURE3D_DESC* texDesc,
    const D3D11_SUBRESOURCE_DATA* initialData,
    const char* name);
ID3D11Texture2D* CreateTexture2D(ResourceArray* resources, const D3D11_TEXTURE2D_DESC* texDesc,
    const D3D11_SUBRESOURCE_DATA* initialData,
    const char* name);
ID3D11RenderTargetView* CreateRenderTargetView(ResourceArray* resources, ID3D11Resource* resource,
    const D3D11_RENDER_TARGET_VIEW_DESC* rtvDesc,
    const char* name);
ID3D11DepthStencilView* CreateDepthStencilView(ResourceArray* resources, ID3D11Resource* resource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC* dsvDesc,
    const char* name);

void ReleaseResources(ResourceArray* resources);

ID3D11ComputeShader* CreateComputeShader(ResourceArray* resources, const void* bytecode, SIZE_T bytecodeLength, const char* name);
ID3D11VertexShader* CreateVertexShader(ResourceArray* resources, const void* bytecode, SIZE_T bytecodeLength, const char* name);
ID3D11PixelShader* CreatePixelShader(ResourceArray* resources, const void* bytecode, SIZE_T bytecodeLength, const char* name);
ID3D11GeometryShader* CreateGeometryShader(ResourceArray* resources, const void* bytecode, SIZE_T bytecodeLength, const char* name);
ID3D11InputLayout* CreateInputLayout(ResourceArray* resources, const VertexBufferId::Type* ids, u32 count,
    const void* shaderBytecodeWithInputSignature,
    SIZE_T bytecodeLength, const char* name);
ID3D11Buffer* CreateBuffer(ResourceArray* resources, const D3D11_BUFFER_DESC* desc, const D3D11_SUBRESOURCE_DATA* initialData, const char* name);

template <size_t N>
ID3D11ComputeShader* CreateComputeShader(ResourceArray* resources, const BYTE (&bytecode)[N], const char* name)
{
    return CreateComputeShader(resources, bytecode, N, name);
}

template <size_t N>
ID3D11VertexShader* CreateVertexShader(ResourceArray* resources, const BYTE (&bytecode)[N], const char* name)
{
    return CreateVertexShader(resources, bytecode, N, name);
}

template <size_t N>
ID3D11PixelShader* CreatePixelShader(ResourceArray* resources, const BYTE (&bytecode)[N], const char* name)
{
    return CreatePixelShader(resources, bytecode, N, name);
}

template <size_t N>
ID3D11GeometryShader* CreateGeometryShader(ResourceArray* resources, const BYTE (&bytecode)[N], const char* name)
{
    return CreateGeometryShader(resources, bytecode, N, name);
}

template <size_t VBSize, size_t ByteCodeSize>
ID3D11InputLayout* CreateInputLayout(ResourceArray* resources, const VertexBufferId::Type (&vbIds)[VBSize], const BYTE (&bytecode)[ByteCodeSize], const char* name)
{
    return CreateInputLayout(resources, vbIds, VBSize, bytecode, ByteCodeSize, name);
}

void InitPipeline(GraphicsPipeline* pipeline);
void SetShaderData(ID3D11Buffer* buffer, const void* data, size_t bytes);
void AppendVertexData(VertexBuffer* buffer, const void* data, u32 itemCount);
void AppendImmutableData(const D3D11_BUFFER_DESC* bDesc, void* data, ID3D11Buffer** buffer);
void DrawIndexed(DrawBuffer* buffer, u32 numIndexes);
void DrawIndexed(DrawBuffer* buffer, u32 numIndexes, u32 indexOffset);
void DrawBuffer_Init(DrawBuffer* buffer);
void DrawBuffer_Allocate(ResourceArray* resources, DrawBuffer* buffer, const char* name);
UINT GetTexture3DSizeBytes(ID3D11Texture3D* texture);
void D3D11_Init(HWND windowHandle);
void D3D11_WindowSizeChanged();
void D3D11_Shutdown();
void ClearDepthStencilBuffer();
void AddImmutableObject(DrawBuffer* buffer, Scene* m);
void AllocateMeshTextures(Scene* mesh);
// Clear render-target and depth-stencil view
// Apply viewport and scissor
// Frame is now ready to get drawn to
void BeginFrame();
// swap buffers
void PresentFrame();

template <typename T>
void SetShaderData(ID3D11Buffer* buffer, const T& data)
{
    SetShaderData(buffer, &data, sizeof(T));
}

template <typename T, size_t N>
void AppendVertexData(VertexBuffer* buffer, T (&data)[N])
{
    AppendVertexData(buffer, data, N);
}

ID3D11Buffer* CreateShaderConstantBuffer(UINT numBytes, const char* name);
void PushViewport(GraphicsPipeline* pipeline, f32 x, f32 y, f32 w, f32 h, f32 zmin = 0.0f, f32 zmax = 1.0f);
void PushScissorRect(GraphicsPipeline* pipeline, LONG x, LONG y, LONG w, LONG h);
void PushViewportAndScissor(GraphicsPipeline* p, f32 x, f32 y, f32 w, f32 h, f32 zmin = 0.0f, f32 zmax = 1.0f);
void SetDefaultViewportAndScissor(GraphicsPipeline* pipeline);
D3D11_SAMPLER_DESC CreateSamplerDesc(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE addressMode, f32 borderColor);
ID3D11RasterizerState* CreateRasterizerState(const D3D11_RASTERIZER_DESC* rasterDesc);
ID3D11RasterizerState2* CreateRasterizerState2(const D3D11_RASTERIZER_DESC2* rasterDesc);
ID3D11BlendState* CreateBlendState(const D3D11_BLEND_DESC* bDesc);
ID3D11SamplerState* CreateSamplerState(const D3D11_SAMPLER_DESC* samplerDesc);
ID3D11DepthStencilState* CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC* desc);

void SetPipeline(GraphicsPipeline* pipeline, bool unbind = true);
void SetPipeline(ComputePipeline* pipeline);
void SetDrawBuffer(GraphicsPipeline* pipeline, DrawBuffer* buffer, const VertexBufferId::Type* ids, s32 count);

template <size_t N>
void SetDrawBuffer(GraphicsPipeline* pipeline, DrawBuffer* buffer, const VertexBufferId::Type (&ids)[N])
{
    SetDrawBuffer(pipeline, buffer, ids, N);
}

struct GBufferShared
{
    ID3D11Texture2D* textures[GBufferId::Count];
    ID3D11RenderTargetView* rtvs[GBufferId::Count];
    ID3D11ShaderResourceView* srvs[GBufferId::Count];
};
extern GBufferShared gBufferShared;

struct SSSOShared
{
    ID3D11Texture2D* tex[2];
    ID3D11ShaderResourceView* SRVs[2];
    ID3D11RenderTargetView* RTVs[2];
};
extern SSSOShared sssoShared;

struct ShadowsSharedData
{
    ID3D11ShaderResourceView* srvs;
};
extern ShadowsSharedData shadowShared;

//
// render passes
//

void Voxel_Init();
void Voxel_Shutdown();
void Voxel_Run(RenderCommandQueue* cmdQueue, DrawBuffer* drawBuffer, Scene* scene, bool runOpacity, bool runEmittance);
void Voxel_DrawViz(RenderCommandQueue* cmdQueue);
void Voxel_SettingsChanged();

void GeometryPass_Init();
void GeometryPass_Shutdown();
void GeometryPass_Draw(RenderCommandQueue* cmdQueue, DrawBuffer* drawBuffer);
void GeometryPass_WindowSizeChanged();

void Shadows_Init();
void Shadows_Shutdown();
void Shadows_Draw(RenderCommandQueue* cmdQueue, DrawBuffer* drawBuffer);

void HiZ_Init();
void HiZ_Run();

void ScreenSpaceSpecularOcclusion_Init();
void ScreenSpaceSpecularOcclusion_Shutdown();
void ScreenSpaceSpecularOcclusion_Run(RenderCommandQueue* cmdQueue, DrawBuffer* drawBuffer);
void ScreenSpaceSpecularOcclusion_WindowSizeChanged();

void SSSO_Filter_Init();
void SSSO_Filter_Draw(RenderCommandQueue* cmdQueue);
void SSSO_Filter_Shutdown();
void SSSO_Filter_WindowSizeChanged();

void DeferredShading_Init();
void DeferredShading_Shutdown();
void DeferredShading_Draw(RenderCommandQueue* cmdQueue, DrawBuffer* drawBuffer);

void DebugViz_Init();
void DebugViz_Shutdown();
struct RenderEntryBox;
void DebugViz_Draw(RenderCommandQueue* cmdQueue);
void DebugViz_AppendBox(RenderCommandQueue* cmdQueue, RenderEntryBox* box);

//
// common
//

void UploadMaterialsToBuffer(DynamicArray<Material>* materials, ID3D11Buffer* buffer);

//
// gui
//

struct ImguiTiming
{
    f32 frame_dt;
};

struct ImguiState
{
    ImguiTiming time_info;
};

struct ConeTracingConstants
{
    f32 cst_opacityThreshold;
    f32 cst_traceStepScale;
    f32 cst_occlusionScale;
    f32 cst_coneAngle;
    f32 cst_maxMipLevel;
    f32 cst_mipBias;
};

struct ShadowMappingConstants
{
    f32 cst_slopeBias;
    f32 cst_fixedBias;
    f32 cst_maxBias;
};

struct TempConstants
{
    f32 temp[4];
};

struct VoxelWireFrameMode
{
    enum Type
    {
        Disabled, // draw voxel face only
        Overlay, // draw voxel face + the wireframe
        Exclusive // draw wireframe only
    };
};

struct VoxelVizConstants
{
    s32 cst_mipLevel;
    s32 cst_wireFrameMode; // see VoxelWireFrameMode::Type
    bool cst_singleChannel;
    s32 emittanceIndex;
};

struct RenderStats
{
    u32 numVoxelizedTriangles; // opacity or emittance, same thing
    u32 numRenderedTriangles; // G-buffer pass
};

extern ConeTracingConstants tracingConstants;
extern ShadowMappingConstants shadowConstants;
extern TempConstants tempConstants;
extern VoxelVizConstants voxelVizConstants;
extern RenderStats renderStats;
extern MemoryPools renderMemory;

struct ConservativeRasterInfo
{
    bool available;
    u32 tier;
};

struct RendererInfo
{
    ConservativeRasterInfo consRasterInfo;
};

struct IndexRange
{
    int start;
    int count;
};

struct BisectData
{
    IndexRange root;
    IndexRange parent;
    IndexRange child;
};

extern BisectData bisect;
extern RendererInfo rendererInfo;

//
// render command queue
//

RenderCommandQueue* AllocateRenderCommandQueue(MemoryArena* arena, size_t size);
