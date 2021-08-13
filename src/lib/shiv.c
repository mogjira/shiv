#include <obsidian/common.h>
#include <obsidian/pipeline.h>
#include <obsidian/renderpass.h>
#include <obsidian/framebuffer.h>
#include <hell/len.h>
#include <hell/hell.h>
#include <string.h>
#include "shiv.h"

typedef Obdn_V_BufferRegion      BufferRegion;
typedef Obdn_V_Command           Command;
typedef Obdn_V_Image             Image;
typedef Obdn_DescriptorBinding DescriptorBinding;

typedef enum {
    PIPELINE_BASIC,
    PIPELINE_WIREFRAME,
    PIPELINE_NO_TEX,
    PIPELINE_DEBUG,
    PIPELINE_UVGRID,
    PIPELINE_COUNT
} PipelineID;

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define SWAP_IMG_COUNT 2
#ifdef SPVDIR_PREFIX 
#define SPVDIR SPVDIR_PREFIX "/shiv"
#else
#define SPVDIR "shiv"
#endif

_Static_assert(SWAP_IMG_COUNT == 2, "SWAP_IMG_COUNT must be 2 for now");

typedef struct {
    Coal_Mat4 view;
    Coal_Mat4 proj;
} Camera;

typedef struct {
    BufferRegion buffer;
    void*        elem[2];
    uint8_t      semaphore;
} ResourceSwapchain;

// we dont use the Obdn_Material because we want to avoid having to do indirect lookups 
// in the shader. Obdn_Material contains handles to textures: we want to convert these 
// into the real texture indices inside the draw function and pass them to the shader 
// as push constants. to do otherwise would require storing the resource maps in some 
// storage buffer which is silly.
typedef struct {
    float r;
    float g;
    float b;
    float roughness;
} Material;

typedef struct {
    Material materials[16];
} MaterialBlock;

typedef struct Shiv_Renderer {
    Obdn_Instance*        instance;
    ResourceSwapchain     cameraUniform;
    ResourceSwapchain     materialUniform;
    uint8_t               texSemaphore;
    PipelineID            curPipeline;
    VkPipeline            graphicsPipelines[PIPELINE_COUNT];
    VkFramebuffer         framebuffers[SWAP_IMG_COUNT];
    VkDescriptorPool      descriptorPool;
    VkDescriptorSet       descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout      pipelineLayout;
    VkFormat              colorFormat;
    VkFormat              depthFormat;
    VkRenderPass          renderPass;
    Vec4                  clearColor;
    VkDevice              device;
} Shiv_Renderer;

void shiv_SetDrawMode(Shiv_Renderer* renderer, const char* arg)
{
    if (strcmp(arg, "wireframe") == 0) 
        renderer->curPipeline = PIPELINE_WIREFRAME;
    else if (strcmp(arg, "basic") == 0)
        renderer->curPipeline = PIPELINE_BASIC;
    else if (strcmp(arg, "notex") == 0)
        renderer->curPipeline = PIPELINE_NO_TEX;
    else if (strcmp(arg, "debug") == 0)
        renderer->curPipeline = PIPELINE_DEBUG;
    else if (strcmp(arg, "uvgrid") == 0)
        renderer->curPipeline = PIPELINE_UVGRID;
    else 
        hell_Print("Options: wireframe basic notex\n");
}

static void changeDrawMode(const Hell_Grimoire* grim, void* data)
{
    Shiv_Renderer* renderer = (Shiv_Renderer*)data;
    const char* arg = hell_GetArg(grim, 1);
    shiv_SetDrawMode(renderer, arg);
}

static void createRenderPasses(VkDevice device, VkFormat colorFormat, VkFormat depthFormat,
        VkImageLayout finalColorLayout, VkImageLayout finalDepthLayout,
        VkRenderPass* mainRenderPass)
{
    assert(mainRenderPass);
    assert(device);

    obdn_CreateRenderPass_ColorDepth(device, VK_IMAGE_LAYOUT_UNDEFINED, finalColorLayout,
            VK_IMAGE_LAYOUT_UNDEFINED, finalDepthLayout,
            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, 
            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
            colorFormat, depthFormat, mainRenderPass);
}

static void createDescriptorSetLayout(VkDevice device, uint32_t maxTextureCount, VkDescriptorSetLayout* layout)
{
    DescriptorBinding bindings[] = {{
            // camera
            .descriptorCount = 1,
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        },{ // materials
            .descriptorCount = 1, // struct of array
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },{ // textures
            .descriptorCount = 16, // arbitrary
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .bindingFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    }};

    Obdn_DescriptorSetInfo dsInfo = {
        .bindingCount = LEN(bindings),
        .bindings = bindings
    };

    obdn_CreateDescriptorSetLayout(device, LEN(bindings), bindings, layout);
}

static void
createPipelineLayout(VkDevice device, const VkDescriptorSetLayout* dsetLayout,
                     VkPipelineLayout*            layout)
{
    uint32_t size1 = sizeof(Mat4) + sizeof(uint32_t) * 3; //prim id, material index, texture index

    const VkPushConstantRange pcPrimId = {
        .offset = 0,
        .size = size1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    };

    // light count
    const VkPushConstantRange pcFrag = {
        .offset = size1,
        .size = sizeof(uint32_t),
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    const VkPushConstantRange ranges[] = {pcPrimId, pcFrag};

    VkPipelineLayoutCreateInfo ci = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = dsetLayout,
        .pushConstantRangeCount = LEN(ranges),
        .pPushConstantRanges    = ranges};

    vkCreatePipelineLayout(device, &ci, NULL, layout);
}

static void
createPipelines(Shiv_Renderer* instance, char* postFragShaderPath, bool openglCompatible, 
        bool countClockwise)
{
    Obdn_GeoAttributeSize attrSizes[3] = {12, 12, 8};

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};

    char* vertshader =
        openglCompatible ? SPVDIR "/opengl.vert.spv" : SPVDIR "/new.vert.spv";

    VkFrontFace frontFace = countClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;


    const Obdn_GraphicsPipelineInfo pipeInfos[] = {{
        // basic
         .renderPass        = instance->renderPass,
         .layout            = instance->pipelineLayout,
         .vertexDescription = obdn_GetVertexDescription(3, attrSizes),
         .polygonMode       = VK_POLYGON_MODE_FILL,
         .frontFace         = frontFace,
         .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
         .sampleCount       = VK_SAMPLE_COUNT_1_BIT,
         .dynamicStateCount = LEN(dynamicStates),
         .pDynamicStates    = dynamicStates,
         .vertShader        = vertshader,
         .fragShader        = SPVDIR"/new.frag.spv"
    },{
        // wireframe 
         .renderPass        = instance->renderPass,
         .layout            = instance->pipelineLayout,
         .vertexDescription = obdn_GetVertexDescription(3, attrSizes),
         .polygonMode       = VK_POLYGON_MODE_LINE,
         .frontFace         = frontFace,
         .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
         .sampleCount       = VK_SAMPLE_COUNT_1_BIT,
         .dynamicStateCount = LEN(dynamicStates),
         .pDynamicStates    = dynamicStates,
         .vertShader        = vertshader,
         .fragShader        = SPVDIR"/new.frag.spv"
    },{
        // notex
         .renderPass        = instance->renderPass,
         .layout            = instance->pipelineLayout,
         .vertexDescription = obdn_GetVertexDescription(3, attrSizes),
         .polygonMode       = VK_POLYGON_MODE_FILL,
         .frontFace         = frontFace,
         .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
         .sampleCount       = VK_SAMPLE_COUNT_1_BIT,
         .dynamicStateCount = LEN(dynamicStates),
         .pDynamicStates    = dynamicStates,
         .vertShader        = vertshader,
         .fragShader        = SPVDIR"/notex.frag.spv"
    },{
        // debug
         .renderPass        = instance->renderPass,
         .layout            = instance->pipelineLayout,
         .vertexDescription = obdn_GetVertexDescription(3, attrSizes),
         .polygonMode       = VK_POLYGON_MODE_FILL,
         .frontFace         = frontFace,
         .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
         .sampleCount       = VK_SAMPLE_COUNT_1_BIT,
         .dynamicStateCount = LEN(dynamicStates),
         .pDynamicStates    = dynamicStates,
         .vertShader        = vertshader,
         .fragShader        = SPVDIR"/debug.frag.spv"
    },{
        // grid uv
         .renderPass        = instance->renderPass,
         .layout            = instance->pipelineLayout,
         .vertexDescription = obdn_GetVertexDescription(3, attrSizes),
         .polygonMode       = VK_POLYGON_MODE_FILL,
         .frontFace         = frontFace,
         .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
         .sampleCount       = VK_SAMPLE_COUNT_1_BIT,
         .dynamicStateCount = LEN(dynamicStates),
         .pDynamicStates    = dynamicStates,
         .vertShader        = vertshader,
         .fragShader        = SPVDIR"/uvgrid.frag.spv"
    }};

    assert(LEN(pipeInfos) == PIPELINE_COUNT);

    obdn_CreateGraphicsPipelines(instance->device, LEN(pipeInfos), pipeInfos,
                                   instance->graphicsPipelines);
}

static void 
createFramebuffer(Shiv_Renderer* renderer, const Obdn_Framebuffer* fb)
{
    assert(fb->aovs[1].aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT);
    VkImageView views[2] = {fb->aovs[0].view, fb->aovs[1].view};
    obdn_CreateFramebuffer(renderer->instance, 2, views, fb->width, fb->height, renderer->renderPass, &renderer->framebuffers[fb->index]);
}


static void
initUniforms(Shiv_Renderer* renderer, Obdn_Memory* memory)
{
    const VkPhysicalDeviceProperties* props = obdn_GetPhysicalDeviceProperties(renderer->instance);
    assert(sizeof(Camera) % props->limits.minUniformBufferOffsetAlignment == 0);
    assert(sizeof(MaterialBlock) % props->limits.minUniformBufferOffsetAlignment == 0);

    renderer->cameraUniform.buffer = obdn_RequestBufferRegion(memory, 2 * sizeof(Camera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, OBDN_V_MEMORY_HOST_GRAPHICS_TYPE);
    renderer->cameraUniform.elem[0] = renderer->cameraUniform.buffer.hostData;
    renderer->cameraUniform.elem[1] = (Camera*)renderer->cameraUniform.buffer.hostData + 1;

    renderer->materialUniform.buffer = obdn_RequestBufferRegion(memory, 2 * sizeof(MaterialBlock), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, OBDN_V_MEMORY_HOST_GRAPHICS_TYPE);
    renderer->materialUniform.elem[0] = renderer->materialUniform.buffer.hostData;
    renderer->materialUniform.elem[1] = (MaterialBlock*)renderer->materialUniform.buffer.hostData + 1;

    VkDescriptorBufferInfo caminfo = {
        .buffer = renderer->cameraUniform.buffer.buffer,
        .offset = renderer->cameraUniform.buffer.offset,
        .range  = renderer->cameraUniform.buffer.size,
    };

    VkDescriptorBufferInfo matinfo = {
        .buffer = renderer->materialUniform.buffer.buffer,
        .offset = renderer->materialUniform.buffer.offset,
        .range  = renderer->materialUniform.buffer.size,
    };

    VkWriteDescriptorSet writes[] = {{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstArrayElement = 0,
        .dstSet = renderer->descriptorSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .pBufferInfo = &caminfo,
    },{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstArrayElement = 0,
        .dstSet = renderer->descriptorSet,
        .dstBinding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .pBufferInfo = &matinfo,
    }};

    vkUpdateDescriptorSets(renderer->device, LEN(writes), writes, 0, NULL);
}

static void 
updateCamera(Shiv_Renderer* renderer, const Obdn_Scene* scene, uint8_t index)
{
    Camera* cam = (Camera*)renderer->cameraUniform.elem[index];
    cam->view = obdn_GetCameraView(scene);
    cam->proj = obdn_GetCameraProjection(scene);
}

static void 
updateMaterialBlock(Shiv_Renderer* renderer, const Obdn_Scene* scene, uint8_t index)
{
    MaterialBlock* matblock = (MaterialBlock*)renderer->materialUniform.elem[index];
    uint32_t count = obdn_SceneGetMaterialCount(scene);
    const Obdn_Material* materials = obdn_SceneGetMaterials(scene);
    for (int i = 0; i < count; i++)
    {
        matblock->materials[i].r = materials[i].color.r;
        matblock->materials[i].g = materials[i].color.g;
        matblock->materials[i].b = materials[i].color.b;
        matblock->materials[i].roughness = materials[i].roughness;
    }
}

static void 
updateTextures(Shiv_Renderer* renderer, const Obdn_Scene* scene, uint8_t index)
{
    uint32_t texCount = obdn_SceneGetTextureCount(scene);
    const Obdn_Texture* textures = obdn_SceneGetTextures(scene);
    for (int i = 0; i < texCount; i++)
    {
        const Obdn_Image* img = &textures[i].devImage;

        VkDescriptorImageInfo textureInfo = {
            .imageLayout = img->layout,
            .imageView   = img->view,
            .sampler     = img->sampler
        };

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = NULL,
            .dstSet = renderer->descriptorSet,
            .dstBinding = 2,
            .dstArrayElement = i,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &textureInfo
        };

        vkUpdateDescriptorSets(renderer->device, 1, &write, 0, NULL);
    }
}

#define MAX_TEXTURE_COUNT 16

void           shiv_CreateRenderer(Obdn_Instance* instance, Obdn_Memory* memory,
                                   VkImageLayout finalColorLayout,
                                   VkImageLayout finalDepthLayout, uint32_t fbCount,
                                   const Obdn_Framebuffer fbs[/*fbCount*/],
                                   const Shiv_Parms* parms, Shiv_Renderer* shiv)
{
    memset(shiv, 0, sizeof(Shiv_Renderer));
    assert(fbCount == SWAP_IMG_COUNT);
    shiv->instance = instance;
    shiv->device   = obdn_GetDevice(instance);

    assert(fbCount == 2);
    assert(fbs[0].aovs[0].aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);
    assert(fbs[0].aovs[1].aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT);
    createRenderPasses(shiv->device, fbs[0].aovs[0].format,
                       fbs[0].aovs[1].format, finalColorLayout, finalDepthLayout,
                        &shiv->renderPass);
    createDescriptorSetLayout(shiv->device, MAX_TEXTURE_COUNT, &shiv->descriptorSetLayout);
    createPipelineLayout(shiv->device, &shiv->descriptorSetLayout, &shiv->pipelineLayout);
    createPipelines(shiv, NULL, parms->openglCompatible, parms->CCWWindingOrder);
    obdn_CreateDescriptorPool(shiv->device, 1, 1, MAX_TEXTURE_COUNT, 0, 0, 0, 0, &shiv->descriptorPool);
    obdn_AllocateDescriptorSets(shiv->device, shiv->descriptorPool, 1, &shiv->descriptorSetLayout, &shiv->descriptorSet);
    for (int i = 0; i < fbCount; i++)
    {
        createFramebuffer(shiv, &fbs[i]);
    }
    initUniforms(shiv, memory);

    if (parms->grim)
    {
        hell_AddCommand(parms->grim, "drawmode", changeDrawMode, shiv);
    }
    shiv->clearColor = parms->clearColor;
}

void
shiv_Render(Shiv_Renderer* renderer, const Obdn_Scene* scene,
            const Obdn_Framebuffer* fb, VkCommandBuffer cmdbuf)
{
    assert(obdn_GetPrimCount(scene));
    // must create framebuffers or find a cached one
    const uint32_t fbi = fb->index;
    const uint32_t width = fb->width;
    const uint32_t height = fb->height;
    assert(fbi < 2);
    if (fb->dirty)
    {
        obdn_DestroyFramebuffer(renderer->instance, renderer->framebuffers[fbi]);
        createFramebuffer(renderer, fb);
    }

    Obdn_SceneDirtyFlags dirt = obdn_GetSceneDirt(scene);
    if (dirt & OBDN_SCENE_CAMERA_VIEW_BIT || dirt & OBDN_SCENE_CAMERA_PROJ_BIT)
    {
        renderer->cameraUniform.semaphore = 2;
    }
    if (dirt & OBDN_SCENE_MATERIALS_BIT)
    {
        renderer->materialUniform.semaphore = 2;
    }
    if (dirt & OBDN_SCENE_TEXTURES_BIT)
    {
        // we block until all queues have finished before updating textures
        // the are not double buffered, so we only need a switch semaphore
        renderer->texSemaphore = 1;
    }

    if (renderer->cameraUniform.semaphore)
    {
        updateCamera(renderer, scene, fbi);
        renderer->cameraUniform.semaphore--;
    }
    if (renderer->materialUniform.semaphore)
    {
        updateMaterialBlock(renderer, scene, fbi);
        renderer->materialUniform.semaphore--;
    }
    if (renderer->texSemaphore)
    {
        // textures are handled differently since we cannot just write into a buffer to handle their updates 
        // we have to actually call vkWriteDescriptorSets(), which means thats we should probably have a separate
        // double buffered descriptor set specifically for textures (or other shader descriptors that cannot be updated 
        // via a simple buffer write)
        // for now, we do the simplest thing which is just block on all vulkan operations before updating.
        vkDeviceWaitIdle(renderer->device); 
        updateTextures(renderer, scene, fbi);
        renderer->texSemaphore--;
    }

    obdn_CmdSetViewportScissorFull(cmdbuf, width, height);

    obdn_CmdBeginRenderPass_ColorDepth(cmdbuf, renderer->renderPass,
                                       renderer->framebuffers[fbi], width,
                                       height, 
                                       renderer->clearColor.r,
                                       renderer->clearColor.g,
                                       renderer->clearColor.b,
                                       renderer->clearColor.a);

    uint32_t uboOffsets[] = {sizeof(Camera) * fbi, sizeof(MaterialBlock) * fbi};
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            renderer->pipelineLayout, 0, 1,
                            &renderer->descriptorSet, LEN(uboOffsets), uboOffsets);

    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->graphicsPipelines[renderer->curPipeline]);

    u32 primCount = obdn_GetPrimCount(scene);
    for (int i = 0; i < primCount; i++)
    {
        const Obdn_Primitive* prim = obdn_GetPrimitive(scene, i);
        //hell_Print("Prim vert count %d\n index count %d\n", prim->geo.vertexCount, prim->geo.indexCount);
        Obdn_Material* mat = obdn_GetMaterial(scene, prim->material);
        uint32_t primIndex = i;
        uint32_t matIndex  = obdn_SceneGetMaterialIndex(scene, prim->material);
        uint32_t texIndex  = obdn_SceneGetTextureIndex(scene, mat->textureAlbedo);
        Mat4 xform = prim->xform; 
        uint32_t indices[] = {primIndex, matIndex, texIndex};
        vkCmdPushConstants(cmdbuf, renderer->pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(xform), &xform);
        vkCmdPushConstants(cmdbuf, renderer->pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, sizeof(xform), sizeof(indices), indices);
        obdn_DrawGeo(cmdbuf, &prim->geo);
    }

    obdn_CmdEndRenderPass(cmdbuf);
}

void 
shiv_DestroyInstance(Shiv_Renderer* instance)
{
}

Shiv_Renderer* shiv_AllocRenderer(void)
{
    return hell_Malloc(sizeof(Shiv_Renderer));
}
