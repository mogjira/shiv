#include <obsidian/common.h>
#include <obsidian/pipeline.h>
#include <obsidian/renderpass.h>
#include <obsidian/framebuffer.h>
#include <hell/len.h>
#include <hell/hell.h>
#include <string.h>

typedef Obdn_V_BufferRegion      BufferRegion;
typedef Obdn_V_Command           Command;
typedef Obdn_V_Image             Image;
typedef Obdn_DescriptorBinding DescriptorBinding;

typedef enum {
    PIPELINE_BASIC,
    PIPELINE_WIREFRAME,
    PIPELINE_COUNT
} PipelineID;

#define SWAP_IMG_COUNT 2
#define SPVDIR "shiv"

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

typedef struct Shiv_Renderer {
    Obdn_Instance*        instance;
    ResourceSwapchain     cameraUniform;
    PipelineID            curPipeline;
    VkPipeline            graphicsPipelines[PIPELINE_COUNT];
    Command               renderCommands[SWAP_IMG_COUNT];
    VkFramebuffer         framebuffers[SWAP_IMG_COUNT];
    VkDescriptorPool      descriptorPool;
    VkDescriptorSet       descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout      pipelineLayout;
    VkFormat              colorFormat;
    VkFormat              depthFormat;
    VkRenderPass          renderPass;
    VkDevice              device;
} Shiv_Renderer;

static void changeDrawMode(const Hell_Grimoire* grim, void* data)
{
    Shiv_Renderer* renderer = (Shiv_Renderer*)data;
    const char* arg = hell_GetArg(grim, 1);
    if (strcmp(arg, "wireframe") == 0) 
        renderer->curPipeline = PIPELINE_WIREFRAME;
    else if (strcmp(arg, "basic") == 0)
        renderer->curPipeline = PIPELINE_BASIC;
    else 
        hell_Print("Options: wireframe basic\n");
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
        },{ // fragment shader whatever
            .descriptorCount = 1,
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
        },{ // textures
            .descriptorCount = maxTextureCount,
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
    VkPushConstantRange pcrs[] = {
        {.offset     = 0,
         .size       = 64, // give 64 bytes for vertex  shader push constant
         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT},
        {.offset     = 64,
         .size       = 64, // give 64 bytes for fragment shader push constant
         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT}};

    VkPipelineLayoutCreateInfo ci = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = dsetLayout,
        .pushConstantRangeCount = LEN(pcrs),
        .pPushConstantRanges    = pcrs};

    vkCreatePipelineLayout(device, &ci, NULL, layout);
}

static void
createPipelines(Shiv_Renderer* instance, char* postFragShaderPath)
{
    Obdn_GeoAttributeSize attrSizes[3] = {12, 12, 8};

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};

    const Obdn_GraphicsPipelineInfo pipeInfos[] = {{
        // basic
         .renderPass        = instance->renderPass,
         .layout            = instance->pipelineLayout,
         .vertexDescription = obdn_GetVertexDescription(3, attrSizes),
         .polygonMode       = VK_POLYGON_MODE_FILL,
         .frontFace         = VK_FRONT_FACE_CLOCKWISE,
         .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
         .sampleCount       = VK_SAMPLE_COUNT_1_BIT,
         .dynamicStateCount = LEN(dynamicStates),
         .pDynamicStates    = dynamicStates,
         .vertShader        = SPVDIR"/basic.vert.spv",
         .fragShader        = SPVDIR"/basic.frag.spv"
    },{
        // wireframe 
         .renderPass        = instance->renderPass,
         .layout            = instance->pipelineLayout,
         .vertexDescription = obdn_GetVertexDescription(3, attrSizes),
         .polygonMode       = VK_POLYGON_MODE_LINE,
         .frontFace         = VK_FRONT_FACE_CLOCKWISE,
         .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
         .sampleCount       = VK_SAMPLE_COUNT_1_BIT,
         .dynamicStateCount = LEN(dynamicStates),
         .pDynamicStates    = dynamicStates,
         .vertShader        = SPVDIR"/basic.vert.spv",
         .fragShader        = SPVDIR"/basic.frag.spv"
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
initCameraUniform(Shiv_Renderer* renderer, Obdn_Memory* memory)
{
    const VkPhysicalDeviceProperties* props = obdn_GetPhysicalDeviceProperties(renderer->instance);
    hell_Print("minubooffsetalignment: %d\n", props->limits.minUniformBufferOffsetAlignment);
    assert(sizeof(Camera) % props->limits.minUniformBufferOffsetAlignment == 0);
    renderer->cameraUniform.buffer = obdn_RequestBufferRegion(memory, 2 * sizeof(Camera), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, OBDN_V_MEMORY_HOST_GRAPHICS_TYPE);
    renderer->cameraUniform.elem[0] = renderer->cameraUniform.buffer.hostData;
    renderer->cameraUniform.elem[1] = (Camera*)renderer->cameraUniform.buffer.hostData + 1;

    VkDescriptorBufferInfo bi = {
        .buffer = renderer->cameraUniform.buffer.buffer,
        .offset = renderer->cameraUniform.buffer.offset,
        .range  = renderer->cameraUniform.buffer.size,
    };

    VkWriteDescriptorSet writes = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstArrayElement = 0,
        .dstSet = renderer->descriptorSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .pBufferInfo = &bi,
    };

    vkUpdateDescriptorSets(renderer->device, 1, &writes, 0, NULL);
}

static void 
updateCamera(Shiv_Renderer* renderer, const Obdn_Scene* scene, uint8_t index)
{
    Camera* cam = (Camera*)renderer->cameraUniform.elem[index];
    cam->view = obdn_GetCameraView(scene);
    cam->proj = obdn_GetCameraProjection(scene);
}

#define MAX_TEXTURE_COUNT 16

void
shiv_CreateRenderer(Obdn_Instance* instance, Obdn_Memory* memory, Hell_Grimoire* grim,
                    VkImageLayout finalColorLayout, VkImageLayout finalDepthLayout,
                    uint32_t fbCount, const Obdn_Framebuffer fbs[fbCount], Shiv_Renderer* shiv)
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
    createPipelines(shiv, NULL);
    obdn_CreateDescriptorPool(shiv->device, 1, 1, MAX_TEXTURE_COUNT, 0, 0, 0, 0, &shiv->descriptorPool);
    obdn_AllocateDescriptorSets(shiv->device, shiv->descriptorPool, 1, &shiv->descriptorSetLayout, &shiv->descriptorSet);
    for (int i = 0; i < fbCount; i++)
    {
        createFramebuffer(shiv, &fbs[i]);
    }
    for (int i = 0; i < fbCount; i++)
    {
        shiv->renderCommands[i] = obdn_CreateCommand(shiv->instance, OBDN_V_QUEUE_GRAPHICS_TYPE);
    }
    initCameraUniform(shiv, memory);

    hell_AddCommand(grim, "drawmode", changeDrawMode, shiv);
}

VkSemaphore
shiv_Render(Shiv_Renderer* renderer, const Obdn_Scene* scene, const Obdn_Framebuffer* fb, uint32_t waitCount,
        VkSemaphore waitSemephores[waitCount])
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

    if (renderer->cameraUniform.semaphore)
    {
        updateCamera(renderer, scene, fbi);
        renderer->cameraUniform.semaphore--;
    }

    Obdn_Command* cmd = &renderer->renderCommands[fbi];
    obdn_WaitForFence(renderer->device, &cmd->fence);
    obdn_ResetCommand(cmd);
    VkCommandBuffer cmdbuf = cmd->buffer;
    obdn_BeginCommandBuffer(cmdbuf);

    obdn_CmdSetViewportScissorFull(cmdbuf, width, height);

    obdn_CmdBeginRenderPass_ColorDepth(cmdbuf, renderer->renderPass,
                                       renderer->framebuffers[fbi], width,
                                       height, 0.0, 0.1, 0.2, 1.0);

    uint32_t uboOffset = sizeof(Camera) * fbi;
    vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            renderer->pipelineLayout, 0, 1,
                            &renderer->descriptorSet, 1, &uboOffset);

    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->graphicsPipelines[renderer->curPipeline]);

    u32 primCount = obdn_GetPrimCount(scene);
    for (int i = 0; i < primCount; i++)
    {
        const Obdn_Primitive* prim = obdn_GetPrimitive(scene, i);
        obdn_DrawGeo(cmdbuf, &prim->geo);
    }

    obdn_CmdEndRenderPass(cmdbuf);
    obdn_EndCommandBuffer(cmdbuf);

    obdn_SubmitGraphicsCommand(
        renderer->instance, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, waitCount,
        waitSemephores, 1, &cmd->semaphore,
        cmd->fence, cmdbuf);

    return cmd->semaphore;
}

void 
shiv_DestroyInstance(Shiv_Renderer* instance)
{
}

Shiv_Renderer* shiv_AllocRenderer(void)
{
    return hell_Malloc(sizeof(Shiv_Renderer));
}
