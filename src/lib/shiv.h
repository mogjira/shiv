#ifndef SHIV_H
#define SHIV_H

#include <obsidian/common.h>
#include <obsidian/s_scene.h>
#include <obsidian/framebuffer.h>

typedef struct Shiv_Renderer Shiv_Renderer;

Shiv_Renderer* shiv_AllocRenderer(void);
void shiv_CreateRenderer(Obdn_Instance* instance, Obdn_Memory* memory, 
                    VkImageLayout finalColorLayout, VkImageLayout finalDepthLayout,
                    uint32_t fbCount, const Obdn_Framebuffer fbs[fbCount], Shiv_Renderer* shiv);
VkSemaphore shiv_Render(Shiv_Renderer* renderer, const Obdn_Scene* scene, const Obdn_Framebuffer* fb, uint32_t waitCount,
        VkSemaphore waitSemephores[waitCount]);
#endif /* end of include guard: SHIV_H */
