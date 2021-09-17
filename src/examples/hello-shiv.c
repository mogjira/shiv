#include <hell/hell.h>
#include <obsidian/obsidian.h>
#include "shiv/shiv.h"

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

Obdn_Command commands[2];

const VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
const VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

static VkSemaphore  acquireSemaphore;

#define WWIDTH  666
#define WHEIGHT 666

static int windowWidth = WWIDTH;
static int windowHeight = WHEIGHT;

#define TARGET_RENDER_INTERVAL 10000 // render every 30 ms

bool handleWindowResizeEvent(const Hell_Event* ev, void* data)
{
    windowWidth= hell_GetWindowResizeWidth(ev);
    windowHeight = hell_GetWindowResizeHeight(ev);
    return false;
}

bool handleMouseEvent(const Hell_Event* ev, void* data)
{
    int mx = ev->data.winData.data.mouseData.x;
    int my = ev->data.winData.data.mouseData.y;
    static bool tumble = false;
    static bool zoom = false;
    static bool pan = false;
    static int xprev = 0;
    static int yprev = 0;
    if (ev->type == HELL_EVENT_TYPE_MOUSEDOWN)
    {
        switch (hell_GetEventButtonCode(ev))
        {
            case HELL_MOUSE_LEFT: tumble = true; break;
            case HELL_MOUSE_MID: pan = true; break;
            case HELL_MOUSE_RIGHT: zoom = true; break;
        }
    }
    if (ev->type == HELL_EVENT_TYPE_MOUSEUP)
    {
        switch (hell_GetEventButtonCode(ev))
        {
            case HELL_MOUSE_LEFT: tumble = false; break;
            case HELL_MOUSE_MID: pan = false; break;
            case HELL_MOUSE_RIGHT: zoom = false; break;
        }
    }
    static Vec3 target = {0,0,0};
    obdn_UpdateCamera_ArcBall(scene, &target, windowWidth, windowHeight, 0.1, xprev, mx, yprev, my, pan, tumble, zoom, false);
    xprev = mx;
    yprev = my;
    return false;
}

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
    Obdn_Command cmd = commands[frameCounter % 2];
    obdn_WaitForFence(obdn_GetDevice(instance), &cmd.fence);
    obdn_ResetCommand(&cmd);
    obdn_BeginCommandBuffer(cmd.buffer);
    shiv_Render(renderer, scene, fb, cmd.buffer);
    obdn_EndCommandBuffer(cmd.buffer);
    obdn_SubmitGraphicsCommand(
        instance, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 1,
        &acquireSemaphore, 1, &cmd.semaphore, cmd.fence, cmd.buffer);
    obdn_PresentFrame(swapchain, 1, &cmd.semaphore);

    obdn_SceneEndFrame(scene);
}

int hellmain(void)
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

    hell_Subscribe(eventQueue, HELL_EVENT_MASK_WINDOW_BIT, hell_GetWindowID(window), handleWindowResizeEvent, NULL);
    hell_Subscribe(eventQueue, HELL_EVENT_MASK_MOUSE_BIT, hell_GetWindowID(window), handleMouseEvent, NULL);

    instance = obdn_AllocInstance();
    memory = obdn_AllocMemory();
    swapchain = obdn_AllocSwapchain();
    scene = obdn_AllocScene();
    const char* testgeopath;
    #if UNIX
    const char* instanceExtensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XCB_SURFACE_EXTENSION_NAME
    };
    testgeopath = "../flip-uv.tnt";
    #elif WIN32
    const char* instanceExtensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };
    obdn_SetRuntimeSpvPrefix("C:/dev/shiv/build/shaders/");
    testgeopath = "C:/dev/dali/data/flip-uv.tnt";
    #endif
    Obdn_InstanceParms ip = {
        .enabledInstanceExentensionCount = 2,
        .ppEnabledInstanceExtensionNames = instanceExtensions
    };
    obdn_CreateInstance(&ip, instance);
    obdn_CreateMemory(instance, 100, 100, 100, 0, 0, memory);
    obdn_CreateScene(grimoire, memory, 0.01, 100, scene);
    obdn_UpdateCamera_LookAt(scene, (Vec3){0, 0, 5}, (Vec3){0,0,0}, (Vec3){0, 1, 0});
    Obdn_AovInfo depthAov = {.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
                             .usageFlags =
                                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             .format = depthFormat};
    obdn_CreateSwapchain(instance, memory, eventQueue, window, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 1, &depthAov, swapchain);
    obdn_LoadPrim(scene, testgeopath, COAL_MAT4_IDENT,(Obdn_MaterialHandle){0}, 0x0);
    renderer = shiv_AllocRenderer();
    Shiv_Parms sp = {
        .grim = grimoire
    };
    shiv_CreateRenderer(instance, memory, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        obdn_GetSwapchainFramebufferCount(swapchain),
                        obdn_GetSwapchainFramebuffers(swapchain), &sp, renderer);
    obdn_CreateSemaphore(obdn_GetDevice(instance), &acquireSemaphore);
    commands[0] = obdn_CreateCommand(instance, OBDN_V_QUEUE_GRAPHICS_TYPE);
    commands[1] = obdn_CreateCommand(instance, OBDN_V_QUEUE_GRAPHICS_TYPE);
    hell_Loop(hellmouth);
    return 0;
}

#ifdef WIN32
int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ PSTR lpCmdLine, _In_ int nCmdShow)
{
    hell_SetHinstance(hInstance);
    hellmain();
    return 0;
}
#elif UNIX
int main(int argc, char* argv[])
{
    hellmain();
}
#endif
