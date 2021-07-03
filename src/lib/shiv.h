#ifndef SHIV_H
#define SHIV_H

#include <obsidian/common.h>
#include <obsidian/scene.h>
#include <obsidian/framebuffer.h>
#include <hell/cmd.h>

typedef struct Shiv_Renderer Shiv_Renderer;

Shiv_Renderer* shiv_AllocRenderer(void);
void shiv_CreateRenderer(Obdn_Instance* instance, Obdn_Memory* memory, Hell_Grimoire* grim,
                    VkImageLayout finalColorLayout, VkImageLayout finalDepthLayout,
                    uint32_t fbCount, const Obdn_Framebuffer fbs[fbCount], Shiv_Renderer* shiv);
void shiv_Render(Shiv_Renderer* renderer, const Obdn_Scene* scene,
            const Obdn_Framebuffer* fb, VkCommandBuffer cmdbuf);
#endif /* end of include guard: SHIV_H */
