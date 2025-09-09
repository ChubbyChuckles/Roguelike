/**
 * @file vec2.h
 * @brief 2D vector mathematics utility.
 *
 * Provides a simple 2D vector structure and basic vector operations
 * for use throughout the game engine. Includes creation, addition,
 * subtraction, and scaling operations implemented as inline functions
 * for optimal performance.
 *
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 */

/*
MIT License

Copyright (c) 2025 ChubbyChuckles

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef ROGUE_MATH_VEC2_H
#define ROGUE_MATH_VEC2_H

/**
 * @brief 2D vector structure.
 *
 * Represents a 2D vector with floating-point x and y components.
 * Used throughout the engine for positions, velocities, directions,
 * and other 2D mathematical operations.
 */
typedef struct RogueVec2
{
    float x; ///< X component of the vector
    float y; ///< Y component of the vector
} RogueVec2;

/**
 * @brief Create a 2D vector from components.
 *
 * Constructs a RogueVec2 with the specified x and y components.
 * This is the primary way to create vector values.
 *
 * @param x The X component
 * @param y The Y component
 * @return A new RogueVec2 with the specified components
 */
static inline RogueVec2 rogue_vec2(float x, float y)
{
    RogueVec2 v = {x, y};
    return v;
}

/**
 * @brief Add two vectors component-wise.
 *
 * Performs vector addition by adding the corresponding components
 * of the two input vectors.
 *
 * @param a First vector
 * @param b Second vector
 * @return The sum vector (a.x + b.x, a.y + b.y)
 */
static inline RogueVec2 rogue_vec2_add(RogueVec2 a, RogueVec2 b)
{
    return rogue_vec2(a.x + b.x, a.y + b.y);
}

/**
 * @brief Subtract two vectors component-wise.
 *
 * Performs vector subtraction by subtracting the corresponding
 * components of the second vector from the first vector.
 *
 * @param a First vector (minuend)
 * @param b Second vector (subtrahend)
 * @return The difference vector (a.x - b.x, a.y - b.y)
 */
static inline RogueVec2 rogue_vec2_sub(RogueVec2 a, RogueVec2 b)
{
    return rogue_vec2(a.x - b.x, a.y - b.y);
}

/**
 * @brief Scale a vector by a scalar value.
 *
 * Multiplies both components of the vector by the given scalar,
 * effectively scaling the vector's magnitude while preserving
 * its direction.
 *
 * @param a The vector to scale
 * @param s The scalar multiplier
 * @return The scaled vector (a.x * s, a.y * s)
 */
static inline RogueVec2 rogue_vec2_scale(RogueVec2 a, float s)
{
    return rogue_vec2(a.x * s, a.y * s);
}

#endif
