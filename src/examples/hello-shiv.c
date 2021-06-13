#include <hell/hell.h>
#include <obsidian/obsidian.h>
#include <obsidian/r_geo.h>
#include "shiv.h"

Hell_Hellmouth*  hellmouth;
Hell_Grimoire*   grimoire;
Hell_Window*     window;
Hell_EventQueue* eventQueue;
Hell_Console*    console;

Obdn_Instance*   instance;
Obdn_Memory*     memory;
Obdn_Swapchain*  swapchain;

Obdn_Scene*      scene;

Shiv_Renderer*   renderer;

const VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
const VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

static VkSemaphore  acquireSemaphore;

#define WWIDTH  666
#define WHEIGHT 666

#define TARGET_RENDER_INTERVAL 1000000 // render every 30 ms

void draw(void)
{
    static Hell_Tick timeOfLastRender = 0;
    static Hell_Tick timeSinceLastRender = TARGET_RENDER_INTERVAL;
    static uint64_t frameCounter = 0;
    timeSinceLastRender = hell_Time() - timeOfLastRender;
    if (timeSinceLastRender < TARGET_RENDER_INTERVAL)
        return;
    timeOfLastRender = hell_Time();
    timeSinceLastRender = 0;

    VkFence fence = VK_NULL_HANDLE;
    const Obdn_Framebuffer* fb = obdn_AcquireSwapchainFramebuffer(swapchain, &fence, &acquireSemaphore);
    VkSemaphore s = shiv_Render(renderer, scene, fb, 1, &acquireSemaphore);
    obdn_PresentFrame(swapchain, 1, &s);

    scene->dirt = 0;
}

int main(int argc, char *argv[])
{
    hellmouth = hell_AllocHellmouth();
    grimoire = hell_AllocGrimoire();
    window = hell_AllocWindow();
    eventQueue = hell_AllocEventQueue();
    console = hell_AllocConsole();
    uint32_t width = WWIDTH;
    uint32_t height = WHEIGHT;
    hell_CreateConsole(console);
    hell_CreateEventQueue(eventQueue);
    hell_CreateGrimoire(eventQueue, grimoire);
    hell_CreateWindow(eventQueue, width, height, NULL, window);
    hell_CreateHellmouth(grimoire, eventQueue, console, 1, &window, draw, NULL, hellmouth);
    instance = obdn_AllocInstance();
    memory = obdn_AllocMemory();
    swapchain = obdn_AllocSwapchain();
    scene = obdn_AllocScene();
    obdn_CreateInstance(false, false, 0, NULL, instance);
    obdn_CreateMemory(instance, 100, 100, 100, 0, 0, memory);
    obdn_CreateScene(WWIDTH, WHEIGHT, 0.01, 100, scene);
    obdn_UpdateCamera_LookAt(scene, (Vec3){0, 0, 10}, (Vec3){0,0,0}, (Vec3){0, 1, 0});
    Obdn_AovInfo depthAov = {.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
                             .usageFlags =
                                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             .format = depthFormat};
    obdn_CreateSwapchain(instance, memory, eventQueue, window, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 1, &depthAov, swapchain);
    obdn_LoadPrim(scene, memory, "pig.tnt", COAL_MAT4_IDENT);
    renderer = shiv_AllocRenderer();
    shiv_CreateRenderer(instance, memory, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        obdn_GetSwapchainFramebufferCount(swapchain),
                        obdn_GetSwapchainFramebuffers(swapchain), renderer);
    obdn_CreateSemaphore(obdn_GetDevice(instance), &acquireSemaphore);
    hell_Loop(hellmouth);
    return 0;
}

