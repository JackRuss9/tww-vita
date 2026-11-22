#include "gpu.h"

#include <stdlib.h>
#include <string.h>

#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>

#include "texture_shaders.h"
#include "gx_probe.h"

/* Exported by libgxm, but missing from the VitaSDK header. */
extern void sceGxmDepthStencilSurfaceSetBackgroundMask(SceGxmDepthStencilSurface *, SceBool);

enum {
    CDRAM_ALIGNMENT = 256 * 1024,
    VERTEX_RING_OFFSET = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE,
    FRAGMENT_RING_OFFSET = VERTEX_RING_OFFSET + SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
    PATCHER_OFFSET = FRAGMENT_RING_OFFSET + SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE,
    GEOMETRY_OFFSET = PATCHER_OFFSET + 64 * 1024,
    TEXTURE_OFFSET = GEOMETRY_OFFSET + 256,
    SCREEN_BYTES = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint32_t),
};

enum Mapping { DRAW_MEMORY, VERTEX_USSE, FRAGMENT_USSE };

typedef struct {
    SceUID id;
    void *base;
    uint32_t usse_offset;
    enum Mapping mapping;
} GpuMemory;

static int allocate_memory(GpuMemory *memory, const char *name, size_t size, enum Mapping mapping)
{
    size = (size + CDRAM_ALIGNMENT - 1) & ~(CDRAM_ALIGNMENT - 1);
    SceUID id = sceKernelAllocMemBlock(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, size, NULL);
    if (id < 0)
        return id;

    void *base;
    int status = sceKernelGetMemBlockBase(id, &base);
    if (status >= 0) {
        switch (mapping) {
        case DRAW_MEMORY:
            status = sceGxmMapMemory(base, size, SCE_GXM_MEMORY_ATTRIB_RW);
            break;
        case VERTEX_USSE:
            status = sceGxmMapVertexUsseMemory(base, size, &memory->usse_offset);
            break;
        case FRAGMENT_USSE:
            status = sceGxmMapFragmentUsseMemory(base, size, &memory->usse_offset);
            break;
        }
    }
    if (status < 0) {
        sceKernelFreeMemBlock(id);
        return status;
    }
    memory->id = id;
    memory->base = base;
    memory->mapping = mapping;
    return 0;
}

static void release_memory(const GpuMemory *memory)
{
    if (!memory->base)
        return;
    switch (memory->mapping) {
    case DRAW_MEMORY: sceGxmUnmapMemory(memory->base); break;
    case VERTEX_USSE: sceGxmUnmapVertexUsseMemory(memory->base); break;
    case FRAGMENT_USSE: sceGxmUnmapFragmentUsseMemory(memory->base); break;
    }
    sceKernelFreeMemBlock(memory->id);
}

static void *patcher_alloc(void *unused, unsigned int size)
{
    (void)unused;
    return malloc(size);
}

static void patcher_free(void *unused, void *memory)
{
    (void)unused;
    free(memory);
}

static const float identity[] = {
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
};

/* Include the stage in the log before a driver call in case it hangs. */
#define GPU_STEP(stage, call) do { \
    probe_log(log, "gpu: %s\n", stage); \
    result = (call); \
    if (result < 0) { \
        probe_log(log, "gpu: %s failed 0x%08x\n", stage, (unsigned int)result); \
        goto cleanup; \
    } \
} while (0)

int gpu_probe(SceUID log, uint32_t *display, size_t display_size)
{
    GxProbeMesh mesh;
    if (gx_probe_mesh(&mesh) < 0) {
        probe_log(log, "gx: FIFO decode failed (%u bytes)\n", (unsigned int)mesh.fifo_bytes);
        return -1;
    }
    probe_log(log, "gx: Aurora FIFO decoded %u bytes, %u vertices, %u indices\n",
              (unsigned int)mesh.fifo_bytes, (unsigned int)mesh.vertex_count, (unsigned int)mesh.index_count);

    GpuMemory draw = {0}, vertex_usse = {0}, fragment_usse = {0}, offscreen = {0};
    GpuMemory fragment_ring = {0};
    SceGxmContext *context = NULL;
    SceGxmRenderTarget *target = NULL;
    SceGxmShaderPatcher *patcher = NULL;
    SceGxmSyncObject *sync = NULL;
    SceGxmShaderPatcherId vertex_id = NULL, fragment_id = NULL;
    SceGxmVertexProgram *vertex_program = NULL;
    SceGxmFragmentProgram *fragment_program = NULL;
    int scene_open = 0, display_mapped = 0, result = -1;
    void *host = malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
    if (!host) {
        probe_log(log, "gpu: context host allocation failed\n");
        return -1;
    }

    GPU_STEP("allocate command memory", allocate_memory(&draw, "twwDraw", GEOMETRY_OFFSET + 4096, DRAW_MEMORY));
    GPU_STEP("allocate vertex USSE", allocate_memory(&vertex_usse, "twwVertex", CDRAM_ALIGNMENT, VERTEX_USSE));
    GPU_STEP("allocate fragment USSE", allocate_memory(&fragment_usse, "twwFragment", CDRAM_ALIGNMENT, FRAGMENT_USSE));
    GPU_STEP("allocate fragment USSE ring", allocate_memory(&fragment_ring, "twwFragmentRing",
        SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE, FRAGMENT_USSE));
    GPU_STEP("allocate offscreen surface", allocate_memory(&offscreen, "twwRender", SCREEN_BYTES, DRAW_MEMORY));
    GPU_STEP("map display surface", sceGxmMapMemory(display, display_size, SCE_GXM_MEMORY_ATTRIB_RW));
    display_mapped = 1;

    unsigned char *commands = draw.base;
    const SceGxmContextParams context_params = {
        .hostMem = host,
        .hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE,
        .vdmRingBufferMem = commands,
        .vdmRingBufferMemSize = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE,
        .vertexRingBufferMem = commands + VERTEX_RING_OFFSET,
        .vertexRingBufferMemSize = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
        .fragmentRingBufferMem = commands + FRAGMENT_RING_OFFSET,
        .fragmentRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE,
        .fragmentUsseRingBufferMem = fragment_ring.base,
        .fragmentUsseRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE,
        .fragmentUsseRingBufferOffset = fragment_ring.usse_offset,
    };
    GPU_STEP("create context", sceGxmCreateContext(&context_params, &context));

    const SceGxmShaderPatcherParams patcher_params = {
        .hostAllocCallback = patcher_alloc,
        .hostFreeCallback = patcher_free,
        .bufferMem = commands + PATCHER_OFFSET,
        .bufferMemSize = GEOMETRY_OFFSET - PATCHER_OFFSET,
        .vertexUsseMem = vertex_usse.base,
        .vertexUsseMemSize = CDRAM_ALIGNMENT,
        .vertexUsseOffset = vertex_usse.usse_offset,
        .fragmentUsseMem = fragment_usse.base,
        .fragmentUsseMemSize = CDRAM_ALIGNMENT,
        .fragmentUsseOffset = fragment_usse.usse_offset,
    };
    GPU_STEP("create shader patcher", sceGxmShaderPatcherCreate(&patcher_params, &patcher));

    const SceGxmProgram *vs = (const SceGxmProgram *)texture_v_gxp;
    const SceGxmProgram *fs = (const SceGxmProgram *)texture_f_gxp;
    GPU_STEP("check vertex shader", sceGxmProgramCheck(vs));
    GPU_STEP("check fragment shader", sceGxmProgramCheck(fs));
    const SceGxmProgramParameter *position = sceGxmProgramFindParameterByName(vs, "aPosition");
    const SceGxmProgramParameter *uv = sceGxmProgramFindParameterByName(vs, "aTexcoord");
    const SceGxmProgramParameter *matrix = sceGxmProgramFindParameterByName(vs, "wvp");
    const SceGxmProgramParameter *sampler = sceGxmProgramFindParameterByName(fs, "tex");
    if (!position || !uv || !matrix || !sampler) {
        probe_log(log, "gpu: texture shader parameters missing\n");
        result = -1;
        goto cleanup;
    }
    const SceGxmVertexAttribute attributes[] = {
        {.offset = offsetof(GxProbeVertex, x), .format = SCE_GXM_ATTRIBUTE_FORMAT_F32,
         .componentCount = 3, .regIndex = sceGxmProgramParameterGetResourceIndex(position)},
        {.offset = offsetof(GxProbeVertex, s), .format = SCE_GXM_ATTRIBUTE_FORMAT_F32,
         .componentCount = 2, .regIndex = sceGxmProgramParameterGetResourceIndex(uv)},
    };
    const SceGxmVertexStream stream = {.stride = sizeof(GxProbeVertex), .indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT};
    GPU_STEP("register vertex shader", sceGxmShaderPatcherRegisterProgram(patcher, vs, &vertex_id));
    GPU_STEP("register fragment shader", sceGxmShaderPatcherRegisterProgram(patcher, fs, &fragment_id));
    GPU_STEP("patch vertex shader", sceGxmShaderPatcherCreateVertexProgram(patcher, vertex_id, attributes, 2,
        &stream, 1, &vertex_program));
    GPU_STEP("patch fragment shader", sceGxmShaderPatcherCreateFragmentProgram(patcher, fragment_id,
        SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4, SCE_GXM_MULTISAMPLE_NONE, NULL, vs, &fragment_program));

    SceGxmColorSurface surface;
    SceGxmDepthStencilSurface depth;
    const SceGxmRenderTargetParams target_params = {
        .width = SCREEN_WIDTH, .height = SCREEN_HEIGHT, .scenesPerFrame = 1, .driverMemBlock = -1,
    };
    GPU_STEP("create render target", sceGxmCreateRenderTarget(&target_params, &target));
    GPU_STEP("create surface sync", sceGxmSyncObjectCreate(&sync));
    GPU_STEP("initialize color surface", sceGxmColorSurfaceInit(&surface, SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
        SCE_GXM_COLOR_SURFACE_LINEAR, SCE_GXM_COLOR_SURFACE_SCALE_NONE, SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
        SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH, offscreen.base));
    GPU_STEP("disable depth surface", sceGxmDepthStencilSurfaceInitDisabled(&depth));
    sceGxmDepthStencilSurfaceSetBackgroundMask(&depth, SCE_TRUE);

    void *gpu_vertices = commands + GEOMETRY_OFFSET;
    void *gpu_indices = commands + GEOMETRY_OFFSET + sizeof(mesh.vertices);
    _Static_assert(GEOMETRY_OFFSET + sizeof(mesh.vertices) + sizeof(mesh.indices) <= TEXTURE_OFFSET,
                   "Texture overlaps geometry");
    memcpy(gpu_vertices, mesh.vertices, sizeof(mesh.vertices));
    memcpy(gpu_indices, mesh.indices, sizeof(mesh.indices));
    uint32_t *texture_data = (uint32_t *)(commands + TEXTURE_OFFSET);
    gx_probe_texture(texture_data);
    probe_log(log, "gx: decoded %ux%u tiled RGB565 texture\n", GX_PROBE_TEXTURE_SIZE, GX_PROBE_TEXTURE_SIZE);
    SceGxmTexture texture;
    GPU_STEP("initialize linear texture", sceGxmTextureInitLinear(&texture, texture_data,
        SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, GX_PROBE_TEXTURE_SIZE, GX_PROBE_TEXTURE_SIZE, 0));
    GPU_STEP("texture clamp S", sceGxmTextureSetUAddrMode(&texture, SCE_GXM_TEXTURE_ADDR_CLAMP));
    GPU_STEP("texture clamp T", sceGxmTextureSetVAddrMode(&texture, SCE_GXM_TEXTURE_ADDR_CLAMP));
    GPU_STEP("texture min point", sceGxmTextureSetMinFilter(&texture, SCE_GXM_TEXTURE_FILTER_POINT));
    GPU_STEP("texture mag point", sceGxmTextureSetMagFilter(&texture, SCE_GXM_TEXTURE_FILTER_POINT));
    memset(offscreen.base, 0, SCREEN_BYTES);
    GPU_STEP("begin scene", sceGxmBeginScene(context, 0, target, NULL, NULL, sync, &surface, &depth));
    scene_open = 1;
    sceGxmSetRegionClip(context, SCE_GXM_REGION_CLIP_NONE, 0, 0, 0, 0);
    sceGxmSetViewport(context, SCREEN_WIDTH * 0.5f, SCREEN_WIDTH * 0.5f,
                   SCREEN_HEIGHT * 0.5f, -SCREEN_HEIGHT * 0.5f, 0.5f, 0.5f);
    sceGxmSetCullMode(context, SCE_GXM_CULL_NONE);
    sceGxmSetFrontDepthFunc(context, SCE_GXM_DEPTH_FUNC_ALWAYS);
    sceGxmSetFrontDepthWriteEnable(context, SCE_GXM_DEPTH_WRITE_DISABLED);
    sceGxmSetVertexProgram(context, vertex_program);
    sceGxmSetFragmentProgram(context, fragment_program);
    void *uniform;
    GPU_STEP("reserve matrix uniform", sceGxmReserveVertexDefaultUniformBuffer(context, &uniform));
    GPU_STEP("set matrix uniform", sceGxmSetUniformDataF(uniform, matrix, 0, 16, identity));
    GPU_STEP("set vertex stream", sceGxmSetVertexStream(context, 0, gpu_vertices));
    GPU_STEP("bind GX texture", sceGxmSetFragmentTexture(context,
        sceGxmProgramParameterGetResourceIndex(sampler), &texture));
    GPU_STEP("draw textured GX quads", sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLES,
        SCE_GXM_INDEX_FORMAT_U16, gpu_indices, mesh.index_count));
    GPU_STEP("end scene", sceGxmEndScene(context, NULL, NULL));
    scene_open = 0;
    probe_log(log, "gpu: wait for draw\n");
    sceGxmFinish(context);

    unsigned int stride = SCREEN_WIDTH * sizeof(uint32_t);
    GPU_STEP("copy offscreen to framebuffer", sceGxmTransferCopy(SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0,
        SCE_GXM_TRANSFER_COLORKEY_NONE, SCE_GXM_TRANSFER_FORMAT_RAW32, SCE_GXM_TRANSFER_LINEAR,
        offscreen.base, 0, 0, stride, SCE_GXM_TRANSFER_FORMAT_RAW32, SCE_GXM_TRANSFER_LINEAR,
        display, 0, 0, stride, NULL, 0, NULL));
    GPU_STEP("wait for copy", sceGxmTransferFinish());

    const size_t samples[] = {
        16 * SCREEN_WIDTH + 16,
        SCREEN_HEIGHT * 3 / 8 * SCREEN_WIDTH + SCREEN_WIDTH * 3 / 8,
        SCREEN_HEIGHT * 3 / 8 * SCREEN_WIDTH + SCREEN_WIDTH * 5 / 8,
        SCREEN_HEIGHT * 5 / 8 * SCREEN_WIDTH + SCREEN_WIDTH * 3 / 8,
        SCREEN_HEIGHT * 5 / 8 * SCREEN_WIDTH + SCREEN_WIDTH * 5 / 8,
    };
    const uint32_t expected[] = {0xff000000, 0xff0000ff, 0xff00ff00, 0xffff0000, 0xff00ffff};
    const volatile uint32_t *rendered = offscreen.base;
    const volatile uint32_t *copied = display;
    int sample_ok = 1;
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        const uint32_t draw_pixel = rendered[samples[i]];
        const uint32_t copy_pixel = copied[samples[i]];
        probe_log(log, "gpu: texture sample %u render %08x copy %08x expected %08x\n",
            (unsigned int)i, (unsigned int)draw_pixel, (unsigned int)copy_pixel, (unsigned int)expected[i]);
        sample_ok &= draw_pixel == expected[i] && copy_pixel == draw_pixel;
    }
    probe_log(log, "gpu: textured framebuffer %s\n", sample_ok ? "pass" : "mismatch");
    result = sample_ok ? 0 : -1;

cleanup:
    if (scene_open)
        sceGxmEndScene(context, NULL, NULL);
    if (context)
        sceGxmFinish(context);
    if (fragment_program)
        sceGxmShaderPatcherReleaseFragmentProgram(patcher, fragment_program);
    if (vertex_program)
        sceGxmShaderPatcherReleaseVertexProgram(patcher, vertex_program);
    if (fragment_id)
        sceGxmShaderPatcherUnregisterProgram(patcher, fragment_id);
    if (vertex_id)
        sceGxmShaderPatcherUnregisterProgram(patcher, vertex_id);
    if (patcher)
        sceGxmShaderPatcherDestroy(patcher);
    if (sync)
        sceGxmSyncObjectDestroy(sync);
    if (target)
        sceGxmDestroyRenderTarget(target);
    if (context)
        sceGxmDestroyContext(context);
    if (display_mapped)
        sceGxmUnmapMemory(display);
    release_memory(&offscreen);
    release_memory(&fragment_ring);
    release_memory(&fragment_usse);
    release_memory(&vertex_usse);
    release_memory(&draw);
    free(host);
    return result;
}
