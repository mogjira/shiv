#ifndef SHIV_H
#define SHIV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <onyx/common.h>
#include <onyx/scene.h>
#include <onyx/frame.h>
#include <hell/cmd.h>
#include <coal/coal.h>

typedef struct Shiv_Renderer Shiv_Renderer;

typedef struct {
    Hell_Grimoire*    grim;
    _Bool             openglCompatible;
    Coal_Vec4         clearColor;
    bool              CCWWindingOrder;
    bool              noBackFaceCull;
} Shiv_Parms;

Shiv_Renderer* shiv_AllocRenderer(void);
void           shiv_CreateRenderer(Onyx_Instance* instance, Onyx_Memory* memory,
                                   VkImageLayout finalColorLayout,
                                   VkImageLayout finalDepthLayout, uint32_t fbCount,
                                   const Onyx_Frame fbs[/*fbCount*/],
                                   const Shiv_Parms* parms, Shiv_Renderer* shiv);
// grim is optional
void shiv_DestroyRenderer(Shiv_Renderer* shiv, Hell_Grimoire* grim);
void shiv_Render(Shiv_Renderer* renderer, const Onyx_Scene* scene,
            const Onyx_Frame* fb, VkCommandBuffer cmdbuf);

void shiv_RenderRegion(Shiv_Renderer* renderer, const Onyx_Scene* scene,
            const Onyx_Frame* fb, uint32_t x, uint32_t y, uint32_t width, uint32_t height, 
            VkCommandBuffer cmdbuf);

void shiv_SetDrawMode(Shiv_Renderer* renderer, const char* arg);

#ifdef __cplusplus
}
#endif

#endif /* end of include guard: SHIV_H */
