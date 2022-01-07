#ifndef SHIV_H
#define SHIV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <obsidian/common.h>
#include <obsidian/scene.h>
#include <obsidian/frame.h>
#include <hell/cmd.h>
#include <coal/coal.h>

typedef struct Shiv_Renderer Shiv_Renderer;

typedef struct {
    Hell_Grimoire*    grim;
    _Bool             openglCompatible;
    Coal_Vec4         clearColor;
    bool              CCWWindingOrder;
} Shiv_Parms;

Shiv_Renderer* shiv_AllocRenderer(void);
void           shiv_CreateRenderer(Obdn_Instance* instance, Obdn_Memory* memory,
                                   VkImageLayout finalColorLayout,
                                   VkImageLayout finalDepthLayout, uint32_t fbCount,
                                   const Obdn_Frame fbs[/*fbCount*/],
                                   const Shiv_Parms* parms, Shiv_Renderer* shiv);
// grim is optional
void shiv_DestroyRenderer(Shiv_Renderer* shiv, Hell_Grimoire* grim);
void shiv_Render(Shiv_Renderer* renderer, const Obdn_Scene* scene,
            const Obdn_Frame* fb, VkCommandBuffer cmdbuf);

void shiv_RenderRegion(Shiv_Renderer* renderer, const Obdn_Scene* scene,
            const Obdn_Frame* fb, uint32_t x, uint32_t y, uint32_t width, uint32_t height, 
            VkCommandBuffer cmdbuf);

void shiv_SetDrawMode(Shiv_Renderer* renderer, const char* arg);

#ifdef __cplusplus
}
#endif

#endif /* end of include guard: SHIV_H */
