#include <hell/hell.h>
#include <obsidian/obsidian.h>
#include "shiv.h"

Hell_Hellmouth*  hellmouth;
Hell_Grimoire*   grimoire;
Hell_Window*     window;
Hell_EventQueue* eventQueue;
Hell_Console*    console;

Obdn_Instance*   instance;
Obdn_Memory*     memory;
Obdn_Swapchain*  swapchain;

Shiv_Renderer*   renderer;

Obdn_Image depthImage;

const VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
const VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

int main(int argc, char *argv[])
{
    hellmouth = hell_AllocHellmouth();
    grimoire = hell_AllocGrimoire();
    window = hell_AllocWindow();
    eventQueue = hell_AllocEventQueue();
    console = hell_AllocConsole();
    uint32_t width = 666;
    uint32_t height = 666;
    hell_CreateConsole(console);
    hell_CreateEventQueue(eventQueue);
    hell_CreateGrimoire(eventQueue, grimoire);
    hell_CreateWindow(eventQueue, width, height, NULL, window);
    hell_CreateHellmouth(grimoire, eventQueue, console, 1, &window, NULL, NULL, hellmouth);
    instance = obdn_AllocInstance();
    memory = obdn_AllocMemory();
    swapchain = obdn_AllocSwapchain();
    obdn_CreateInstance(true, false, 0, NULL, instance);
    obdn_CreateMemory(instance, 100, 100, 100, 0, 0, memory);
    obdn_CreateSwapchain(instance, eventQueue, window, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, swapchain);
    obdn_CreateImage(memory, width, height, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, VK_SAMPLE_COUNT_1_BIT, 1, OBDN_V_MEMORY_DEVICE_TYPE);
    renderer = shiv_AllocRenderer();
    shiv_CreateRenderer(instance, memory, obdn_GetSwapchainWidth(swapchain),
                        obdn_GetSwapchainHeight(swapchain),
                        obdn_GetSwapchainFormat(swapchain), VK_FORMAT_D24_UNORM_S8_UINT,
                        depthImage.view, obdn_GetSwapchainImageCount(swapchain),
                        obdn_GetSwapchainImageViews(swapchain), renderer);
    hell_Loop(hellmouth);
    return 0;
}
