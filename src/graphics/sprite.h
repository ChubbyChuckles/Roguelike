/**
 * @file sprite.h
 * @brief Sprite and texture management system.
 *
 * Provides data structures and functions for loading, managing, and rendering
 * textures and sprites. Handles texture loading from files, sprite definition
 * from texture regions, and basic sprite rendering operations. The system
 * is designed to work with or without SDL depending on build configuration.
 *
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 */

#ifndef ROGUE_GRAPHICS_SPRITE_H
#define ROGUE_GRAPHICS_SPRITE_H

#include <stdbool.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/**
 * @brief Texture resource structure.
 *
 * Represents a loaded texture with its dimensions and platform-specific
 * handle. When SDL is available, contains an SDL_Texture handle.
 * The structure tracks texture dimensions for rendering calculations.
 */
typedef struct RogueTexture
{
#ifdef ROGUE_HAVE_SDL
    SDL_Texture* handle; ///< SDL texture handle (when SDL is available)
#endif
    int w; ///< Texture width in pixels
    int h; ///< Texture height in pixels
} RogueTexture;

/**
 * @brief Sprite definition structure.
 *
 * Defines a sprite as a rectangular region within a texture.
 * Sprites allow multiple graphics elements to be stored in a single
 * texture (sprite sheet) and individually rendered by specifying
 * their source rectangle coordinates.
 */
typedef struct RogueSprite
{
    RogueTexture* tex; ///< Pointer to the source texture
    int sx;            ///< Source X coordinate in texture
    int sy;            ///< Source Y coordinate in texture  
    int sw;            ///< Source width in texture
    int sh;            ///< Source height in texture
} RogueSprite;

/**
 * @brief Load a texture from a file.
 *
 * Loads an image file into a texture that can be used for rendering.
 * Supports common image formats depending on the underlying graphics
 * system. The texture must be destroyed with rogue_texture_destroy()
 * when no longer needed.
 *
 * @param t Pointer to the texture structure to populate
 * @param path File path to the image to load
 * @return true on successful load, false on failure
 */
bool rogue_texture_load(RogueTexture* t, const char* path);

/**
 * @brief Destroy and free a texture.
 *
 * Releases the graphics resources used by a texture and resets
 * the texture structure. After calling this function, the texture
 * is no longer valid for rendering operations.
 *
 * @param t Pointer to the texture to destroy
 */
void rogue_texture_destroy(RogueTexture* t);

/**
 * @brief Draw a sprite at the specified position.
 *
 * Renders the sprite to the current rendering target at the given
 * screen coordinates. The sprite can be scaled by an integer factor
 * for pixel-perfect scaling.
 *
 * @param spr Pointer to the sprite to draw
 * @param x Screen X coordinate for the top-left corner
 * @param y Screen Y coordinate for the top-left corner
 * @param scale Integer scaling factor (1 = original size, 2 = double size, etc.)
 */
void rogue_sprite_draw(const RogueSprite* spr, int x, int y, int scale);

#endif
