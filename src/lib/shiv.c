#include <obsidian/common.h>
#include <obsidian/r_pipeline.h>
#include <obsidian/r_renderpass.h>
#include <hell/len.h>
#include <hell/hell.h>
#include <string.h>

typedef Obdn_V_BufferRegion      BufferRegion;
typedef Obdn_V_Command           Command;
typedef Obdn_V_Image             Image;
typedef Obdn_DescriptorBinding DescriptorBinding;

enum {
    PIPELINE_RASTER,
    PIPELINE_POST,
    PIPELINE_COUNT
};

#define SWAP_IMG_COUNT 2
#define SPVDIR "shiv"

typedef struct Shiv_Renderer {
    Obdn_Instance*        instance;
    BufferRegion          cameraRegion;
    VkPipeline            graphicsPipelines[PIPELINE_COUNT];
    //uint32_t              graphicsQueueFamilyIndex;
    Command               renderCommand;
    VkFramebuffer         mainFrameBuffers[SWAP_IMG_COUNT];
    VkFramebuffer         postFrameBuffers[SWAP_IMG_COUNT];
    VkDescriptorPool      descriptorPool;
    VkDescriptorSet       descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout      pipelineLayout;
    VkFormat              colorFormat;
    VkFormat              depthFormat;
    VkRenderPass          mainRenderPass;
    VkRenderPass          postRenderPass;
    VkDevice              device;
} Shiv_Renderer;

static void createRenderPasses(VkDevice device, VkFormat colorFormat, VkFormat depthFormat,
        VkRenderPass* mainRenderPass, VkRenderPass* postRenderPass)
{
    assert(mainRenderPass);
    assert(postRenderPass);
    assert(device);

    obdn_CreateRenderPass_ColorDepth(device, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, 
            VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
            colorFormat, depthFormat, mainRenderPass);

    obdn_CreateRenderPass_Color(device,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_ATTACHMENT_LOAD_OP_LOAD, colorFormat, postRenderPass);
}

static void createDescriptorSetLayout(VkDevice device, uint32_t maxTextureCount, VkDescriptorSetLayout* layout)
{
    DescriptorBinding bindings[] = {{
            // camera
            .descriptorCount = 1,
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
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
    Obdn_R_AttributeSize attrSizes[3] = {12, 12, 8};

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};

    const Obdn_GraphicsPipelineInfo pipeInfosGraph[] = {
        {// raster
         .renderPass        = instance->mainRenderPass,
         .layout            = instance->pipelineLayout,
         .vertexDescription = obdn_r_GetVertexDescription(3, attrSizes),
         .frontFace         = VK_FRONT_FACE_CLOCKWISE,
         .sampleCount       = VK_SAMPLE_COUNT_1_BIT,
         .dynamicStateCount = LEN(dynamicStates),
         .pDynamicStates    = dynamicStates,
         .vertShader        = SPVDIR"/basic.vert.spv",
         .fragShader        = SPVDIR"/basic.frag.spv"},
        {// post
         .renderPass        = instance->postRenderPass,
         .layout            = instance->pipelineLayout,
         .sampleCount       = VK_SAMPLE_COUNT_1_BIT,
         .frontFace         = VK_FRONT_FACE_CLOCKWISE,
         .blendMode         = OBDN_R_BLEND_MODE_OVER,
         .dynamicStateCount = LEN(dynamicStates),
         .pDynamicStates    = dynamicStates,
         .vertShader        = OBDN_FULL_SCREEN_VERT_SPV,
         .fragShader        = postFragShaderPath ? postFragShaderPath
                                                 : "post.frag.spv"}};

    obdn_CreateGraphicsPipelines(instance->device, LEN(pipeInfosGraph), pipeInfosGraph,
                                   instance->graphicsPipelines);
}

static void
createFramebuffers(Shiv_Renderer* shiv, uint32_t width, uint32_t height,
                   const VkImageView depthView, const VkImageView colorViews[2])
{
    VkImageView attachments[] = {colorViews[0], depthView};
    obdn_CreateFramebuffer(shiv->instance, LEN(attachments), attachments, width, height, shiv->mainRenderPass, &shiv->mainFrameBuffers[0]);
    attachments[0] = colorViews[1];
    obdn_CreateFramebuffer(shiv->instance, LEN(attachments), attachments, width, height, shiv->mainRenderPass, &shiv->mainFrameBuffers[1]);
    obdn_CreateFramebuffer(shiv->instance, 1, &colorViews[0], width, height, shiv->postRenderPass, &shiv->postFrameBuffers[0]);
    obdn_CreateFramebuffer(shiv->instance, 1, &colorViews[1], width, height, shiv->postRenderPass, &shiv->postFrameBuffers[1]);
}

#define MAX_TEXTURE_COUNT 16

void
shiv_CreateRenderer(Obdn_Instance* instance, Obdn_Memory* memory, uint32_t width, uint32_t height,
                    VkFormat colorFormat, VkFormat depthFormat, VkImageView depthAttachment, uint32_t viewCount,
                    const VkImageView colorAttachments[viewCount], Shiv_Renderer* shiv)
{
    assert(viewCount == SWAP_IMG_COUNT);
    memset(shiv, 0, sizeof(Shiv_Renderer));
    shiv->instance = instance;
    shiv->device   = obdn_GetDevice(instance);
    shiv->renderCommand = obdn_CreateCommand(shiv->instance, OBDN_V_QUEUE_GRAPHICS_TYPE);

    createRenderPasses(shiv->device, colorFormat, depthFormat, &shiv->mainRenderPass, &shiv->postRenderPass);
    createDescriptorSetLayout(shiv->device, MAX_TEXTURE_COUNT, &shiv->descriptorSetLayout);
    createPipelineLayout(shiv->device, &shiv->descriptorSetLayout, &shiv->pipelineLayout);
    createPipelines(shiv, NULL);
    obdn_CreateDescriptorPool(shiv->device, 1, MAX_TEXTURE_COUNT, 0, 0, 0, 0, &shiv->descriptorPool);
    obdn_AllocateDescriptorSets(shiv->device, shiv->descriptorPool, 1, &shiv->descriptorSetLayout, &shiv->descriptorSet);
    createFramebuffers(shiv, width, height, depthAttachment, colorAttachments);
}

void 
shiv_DestroyInstance(Shiv_Renderer* instance)
{
}



Shiv_Renderer* shiv_AllocRenderer(void)
{
    return hell_Malloc(sizeof(Shiv_Renderer));
}
