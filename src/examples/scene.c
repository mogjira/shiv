#define COAL_SIMPLE_TYPE_NAMES
#include <hell/hell.h>
#include <unistd.h>
#include <obsidian/obsidian.h>
#include <string.h>
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

Obdn_Image    textures[10];
Obdn_Geometry geos[10];
uint32_t      primCount;
Obdn_Geometry geo;

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

// TODO: Take an argument here to a texture path on disk. And modify LoadTexture to have 
// an error return code if the texture is not found
void addprim(Hell_Grimoire* grim, void* scenedata)
{
    int argc = hell_GetArgC(grim);
    hell_Print("Argc %d\n", argc);
    if (hell_GetArgC(grim) != 3)
    {
        hell_Print("Must provide a name of the prim [cube] and path to an image to be used as the color texture.\n");
        return;
    }
    const char* primName = hell_GetArg(grim, 1);
    if (strncmp(primName, "cube", 4) != 0)
    {
        hell_Print("Invalid prim specified.\n");
        return;
    }
    const char* path = hell_GetArg(grim, 2);
    if (access(path, R_OK) != 0) 
    {
        hell_Print("Texture file either does not exist or does not grant read permission.\n");
        return;
    }

    static int x = 0;
    Obdn_Scene* scene = (Obdn_Scene*)scenedata;
    Coal_Mat4 xform = COAL_MAT4_IDENT;
    Coal_Vec3 t = {x, 0, 0};
    xform = coal_Translate_Mat4(t, xform);
    obdn_LoadImage(
        memory, path, 4,
        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT, VK_SAMPLE_COUNT_1_BIT, VK_FILTER_LINEAR,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true, OBDN_MEMORY_DEVICE_TYPE, &textures[primCount]);
    Obdn_TextureHandle tex = obdn_SceneAddTexture(scene, &textures[primCount]);
    Obdn_MaterialHandle mat = obdn_SceneCreateMaterial(scene, (Vec3){1, 1, 1}, 0.3, tex, NULL_TEXTURE, NULL_TEXTURE);
    geos[primCount] = obdn_CreateCube(memory, true);
    obdn_SceneAddPrim(scene, &geos[primCount], xform, mat);
    primCount++;
    assert(primCount < 10); //arbitrary
    x += 1;
}

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

void draw(u64 fi, u64 dt)
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
    const Obdn_Frame* fb = obdn_AcquireSwapchainFrame(swapchain, &fence, &acquireSemaphore);
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

    hell_Subscribe(eventQueue, HELL_EVENT_MASK_WINDOW_BIT,
                   hell_GetWindowID(window), handleWindowResizeEvent, NULL);
    hell_Subscribe(eventQueue, HELL_EVENT_MASK_POINTER_BIT,
                   hell_GetWindowID(window), handleMouseEvent, NULL);

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
    obdn_CreateScene(grimoire, memory, 1, 1, 0.01, 100, scene);
    obdn_UpdateCamera_LookAt(scene, (Vec3){0, 0, 5}, (Vec3){0,0,0}, (Vec3){0, 1, 0});
    Obdn_AovInfo depthAov = {.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
                             .usageFlags =
                                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             .format = depthFormat};
    obdn_CreateSwapchain(instance, memory, eventQueue, window,
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 1, &depthAov,
                         swapchain);
    geo = obdn_LoadGeo(memory, 0, testgeopath, true);
    obdn_SceneAddPrim(scene, &geo, COAL_MAT4_IDENT, (Obdn_MaterialHandle){0});
    renderer = shiv_AllocRenderer();
    Shiv_Parms sp = {
        .grim = grimoire
    };
    shiv_CreateRenderer(instance, memory, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        obdn_GetSwapchainFrameCount(swapchain),
                        obdn_GetSwapchainFrames(swapchain), &sp, renderer);
    obdn_CreateSemaphore(obdn_GetDevice(instance), &acquireSemaphore);
    hell_AddCommand(grimoire, "addprim", addprim, scene);

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
