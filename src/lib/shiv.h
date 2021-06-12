#ifndef SHIV_H
#define SHIV_H

#include <obsidian/common.h>
#include <obsidian/s_scene.h>

typedef struct Shiv_Renderer Shiv_Renderer;

Shiv_Renderer* shiv_AllocRenderer(void);
void shiv_CreateRenderer(Obdn_Instance* instance, Obdn_Memory* memory, uint32_t width, uint32_t height,
                    VkFormat colorFormat, VkFormat depthFormat, VkImageView depthAttachment, uint32_t viewCount,
                    const VkImageView colorAttachments[viewCount], Shiv_Renderer* shiv);

#endif /* end of include guard: SHIV_H */
