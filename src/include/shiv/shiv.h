#ifndef SHIV_H
#define SHIV_H

#include <obsidian/common.h>
#include <obsidian/scene.h>
#include <obsidian/framebuffer.h>
#include <hell/cmd.h>
#include <coal/coal.h>

typedef struct Shiv_Renderer Shiv_Renderer;

typedef struct {
    Hell_Grimoire*    grim;
    _Bool             openglCompatible;
    Vec4              clearColor;
} Shiv_Parms;

Shiv_Renderer* shiv_AllocRenderer(void);
void           shiv_CreateRenderer(Obdn_Instance* instance, Obdn_Memory* memory,
                                   VkImageLayout finalColorLayout,
                                   VkImageLayout finalDepthLayout, uint32_t fbCount,
                                   const Obdn_Framebuffer fbs[/*fbCount*/],
                                   const Shiv_Parms* parms, Shiv_Renderer* shiv);
void shiv_Render(Shiv_Renderer* renderer, const Obdn_Scene* scene,
            const Obdn_Framebuffer* fb, VkCommandBuffer cmdbuf);

void shiv_SetDrawMode(Shiv_Renderer* renderer, const char* arg);

#endif /* end of include guard: SHIV_H */
