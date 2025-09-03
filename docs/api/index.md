@page api_reference_hub API Reference Hub

# API Reference Hub

Welcome to the comprehensive API reference for the Roguelike Engine. This hub provides detailed documentation for all public APIs, data structures, and integration points.

## 📋 API Organization

### Core Systems
- **[Entity System](entity_system.md)** - Entity management and component architecture
- **[Combat System](combat_system.md)** - Attack mechanics and damage calculation
- **[World System](world_system.md)** - Terrain generation and spatial management
- **[Audio System](audio_system.md)** - Sound playback and spatial audio
- **[UI System](ui_system.md)** - Interface rendering and input handling

### Game Systems
- **[Character System](character_system.md)** - Player and NPC management
- **[Inventory System](inventory_system.md)** - Item storage and management
- **[Skill System](skill_system.md)** - Ability framework and progression
- **[Crafting System](crafting_system.md)** - Item creation and material processing
- **[Economy System](economy_system.md)** - Trading and vendor mechanics

### Utility Systems
- **[Math Utilities](math_utilities.md)** - Vector operations and calculations
- **[File I/O](file_io.md)** - Asset loading and persistence
- **[Memory Management](memory_management.md)** - Allocation and pooling
- **[Debug Tools](debug_tools.md)** - Development and profiling utilities
- **[Configuration](configuration.md)** - Settings and runtime parameters

## 🔧 Quick Reference

### Common Patterns

#### Initialization and Cleanup
```c
// Standard initialization pattern
RogueSystem* system = rogue_system_create();
if (!system) {
    // Handle allocation failure
    return ROGUE_ERROR_OUT_OF_MEMORY;
}

// Use system...

// Always cleanup
rogue_system_destroy(system);
```

#### Error Handling
```c
RogueError result = rogue_operation_perform(params);
switch (result) {
    case ROGUE_SUCCESS:
        // Operation succeeded
        break;
    case ROGUE_ERROR_INVALID_PARAMETER:
        // Handle invalid parameter
        break;
    case ROGUE_ERROR_OUT_OF_MEMORY:
        // Handle memory allocation failure
        break;
    default:
        // Handle unexpected error
        break;
}
```

#### Resource Management
```c
// RAII-style resource management
void process_data(const char* filename) {
    RogueFile* file = rogue_file_open(filename, ROGUE_FILE_READ);
    if (!file) return;

    // Process file contents...

    rogue_file_close(file); // Always cleanup
}
```

### Callback Registration
```c
// Event callback pattern
void on_combat_event(RogueCombatEvent* event, void* user_data) {
    MyGameState* game = (MyGameState*)user_data;

    switch (event->type) {
        case ROGUE_COMBAT_DAMAGE_DEALT:
            update_ui_damage(event->damage);
            break;
        case ROGUE_COMBAT_ENTITY_DIED:
            handle_entity_death(event->entity_id);
            break;
    }
}

// Register callback
rogue_combat_register_callback(on_combat_event, game_state);
```

## 📚 Data Structures

### Core Types

#### Entity System
```c
typedef struct {
    RogueEntityId id;
    RogueVector2 position;
    RogueVector2 velocity;
    float rotation;
    RogueEntityFlags flags;
} RogueEntity;

typedef struct {
    RogueComponentType type;
    void* data;
    size_t size;
} RogueComponent;
```

#### Combat System
```c
typedef struct {
    int base_damage;
    float damage_multiplier;
    RogueDamageType damage_type;
    RogueAttackFlags flags;
    RogueHitbox hitbox;
} RogueAttackDefinition;

typedef struct {
    RogueEntityId attacker;
    RogueEntityId defender;
    int damage_dealt;
    RogueDamageType damage_type;
    bool critical_hit;
    bool blocked;
} RogueCombatResult;
```

#### World System
```c
typedef struct {
    int x, y;           // Grid coordinates
    RogueTileType type; // Terrain type
    uint32_t flags;     // Tile properties
    void* user_data;    // Custom data
} RogueTile;

typedef struct {
    RogueVector2 bounds_min;
    RogueVector2 bounds_max;
    RogueTile* tiles;
    int width, height;
    uint32_t flags;
} RogueWorldChunk;
```

### Collections and Containers

#### Dynamic Arrays
```c
typedef struct {
    void* data;
    size_t element_size;
    size_t capacity;
    size_t count;
} RogueArray;

RogueArray* array = rogue_array_create(sizeof(MyType), initial_capacity);
rogue_array_push(array, &my_item);
MyType* item = rogue_array_get(array, index);
rogue_array_destroy(array);
```

#### Hash Maps
```c
typedef struct {
    void* key_data;
    void* value_data;
    size_t key_size;
    size_t value_size;
    size_t capacity;
    RogueHashFunction hash_func;
} RogueHashMap;

RogueHashMap* map = rogue_hashmap_create(sizeof(KeyType), sizeof(ValueType));
rogue_hashmap_set(map, &key, &value);
ValueType* result = rogue_hashmap_get(map, &key);
rogue_hashmap_destroy(map);
```

## 🔄 Function Reference

### Naming Conventions

#### Function Categories
```
Creation/Destruction:
├── rogue_system_create() / rogue_system_destroy()
├── rogue_entity_spawn() / rogue_entity_despawn()
└── rogue_resource_load() / rogue_resource_unload()

State Management:
├── rogue_system_update()
├── rogue_entity_update()
└── rogue_world_tick()

Queries and Getters:
├── rogue_entity_get_position()
├── rogue_combat_get_health()
└── rogue_inventory_get_item_count()

Actions and Commands:
├── rogue_entity_move()
├── rogue_combat_attack()
└── rogue_inventory_add_item()

Utilities:
├── rogue_math_distance()
├── rogue_string_format()
└── rogue_random_generate()
```

#### Parameter Patterns
```c
// Input parameters (const)
void rogue_entity_set_position(RogueEntity* entity, const RogueVector2* position);

// Output parameters (pointer to modifiable)
void rogue_entity_get_position(const RogueEntity* entity, RogueVector2* out_position);

// Input/output parameters (pointer to modifiable with initial value)
void rogue_entity_move(RogueEntity* entity, RogueVector2* velocity, float delta_time);

// Optional parameters (nullable pointers)
void rogue_audio_play_sound(const char* sound_name, const RogueVector2* position, float* volume);
```

### Error Handling

#### Error Codes
```c
typedef enum {
    ROGUE_SUCCESS = 0,
    ROGUE_ERROR_INVALID_PARAMETER = -1,
    ROGUE_ERROR_OUT_OF_MEMORY = -2,
    ROGUE_ERROR_NOT_FOUND = -3,
    ROGUE_ERROR_ALREADY_EXISTS = -4,
    ROGUE_ERROR_PERMISSION_DENIED = -5,
    ROGUE_ERROR_IO_ERROR = -6,
    ROGUE_ERROR_TIMEOUT = -7,
    ROGUE_ERROR_NOT_SUPPORTED = -8
} RogueError;
```

#### Error Context
```c
typedef struct {
    RogueError code;
    const char* message;
    const char* file;
    int line;
    void* context;
} RogueErrorInfo;

// Get detailed error information
RogueErrorInfo info;
rogue_get_last_error(&info);
printf("Error %d at %s:%d: %s\n", info.code, info.file, info.line, info.message);
```

## 🔌 Integration Examples

### Basic Game Loop
```c
#include <roguelike.h>

int main(int argc, char** argv) {
    // Initialize engine
    RogueEngine* engine = rogue_engine_create();
    if (!engine) return 1;

    // Create world
    RogueWorld* world = rogue_world_create(100, 100);
    rogue_engine_set_world(engine, world);

    // Create player
    RogueEntity* player = rogue_entity_create(ENTITY_TYPE_PLAYER);
    rogue_entity_set_position(player, (RogueVector2){50.0f, 50.0f});
    rogue_world_add_entity(world, player);

    // Main game loop
    while (!rogue_engine_should_quit(engine)) {
        // Handle input
        rogue_input_update();

        // Update game state
        rogue_engine_update(engine, 16.67f); // 60 FPS

        // Render
        rogue_renderer_clear();
        rogue_world_render(world);
        rogue_renderer_present();
    }

    // Cleanup
    rogue_engine_destroy(engine);
    return 0;
}
```

### Combat System Integration
```c
// Combat event handler
void on_damage_dealt(RogueCombatEvent* event, void* user_data) {
    MyGameState* game = (MyGameState*)user_data;

    // Update UI
    update_damage_numbers(event->position, event->damage);

    // Play sound effect
    if (event->critical_hit) {
        rogue_audio_play_sound("critical_hit", &event->position, NULL);
    }

    // Check for death
    if (rogue_entity_get_health(event->defender) <= 0) {
        handle_entity_death(event->defender);
    }
}

// Setup combat system
RogueCombatSystem* combat = rogue_combat_system_create();
rogue_combat_register_callback(combat, ROGUE_COMBAT_DAMAGE_DEALT,
                              on_damage_dealt, game_state);

// Execute attack
RogueCombatResult result;
RogueError error = rogue_combat_attack(combat, player_entity, target_entity, &result);
if (error == ROGUE_SUCCESS) {
    // Attack succeeded
    apply_attack_effects(&result);
}
```

### Inventory Management
```c
// Inventory operations
RogueInventory* inventory = rogue_entity_get_inventory(player_entity);

// Add item
RogueItem item = {ITEM_TYPE_SWORD, 1};
RogueError error = rogue_inventory_add_item(inventory, &item);
if (error != ROGUE_SUCCESS) {
    // Handle inventory full or other errors
    return error;
}

// Find item
RogueItem* found_item = NULL;
size_t item_index = 0;
if (rogue_inventory_find_item(inventory, ITEM_TYPE_POTION, &found_item, &item_index)) {
    // Use potion
    rogue_inventory_remove_item(inventory, item_index);
    apply_potion_effect(found_item);
}

// Equipment management
RogueEquipmentSlot slot = EQUIPMENT_SLOT_WEAPON;
RogueError equip_error = rogue_inventory_equip_item(inventory, item_index, slot);
if (equip_error == ROGUE_SUCCESS) {
    update_player_stats();
}
```

## 📊 Performance Guidelines

### Memory Management
```c
// Efficient memory usage patterns
#define MAX_ENTITIES 1024
#define ENTITY_POOL_SIZE 256

// Pre-allocate pools for common objects
RogueEntityPool* entity_pool = rogue_entity_pool_create(ENTITY_POOL_SIZE);

// Reuse objects when possible
RogueEntity* entity = rogue_entity_pool_acquire(entity_pool);
if (!entity) {
    // Pool exhausted, allocate new
    entity = rogue_entity_create();
}

// Return to pool when done
rogue_entity_pool_release(entity_pool, entity);
```

### Update Optimization
```c
// Batch operations for better performance
void update_entities(RogueWorld* world, float delta_time) {
    const size_t entity_count = rogue_world_get_entity_count(world);

    // Process in batches to improve cache locality
    for (size_t i = 0; i < entity_count; i += 64) {
        const size_t batch_end = ROGUE_MIN(i + 64, entity_count);

        for (size_t j = i; j < batch_end; j++) {
            RogueEntity* entity = rogue_world_get_entity(world, j);
            rogue_entity_update(entity, delta_time);
        }
    }
}
```

### Threading Considerations
```c
// Thread-safe API usage
RogueMutex* world_mutex = rogue_mutex_create();

// In worker thread
rogue_mutex_lock(world_mutex);
rogue_world_update_chunk(world, chunk_x, chunk_y);
rogue_mutex_unlock(world_mutex);

// Main thread can safely read
rogue_mutex_lock(world_mutex);
render_world_chunk(world, chunk_x, chunk_y);
rogue_mutex_unlock(world_mutex);

rogue_mutex_destroy(world_mutex);
```

## 🔍 Debugging and Diagnostics

### Logging System
```c
// Configure logging
rogue_log_set_level(ROGUE_LOG_LEVEL_DEBUG);
rogue_log_set_output_file("game.log");

// Log messages
rogue_log_debug("Entity %d spawned at (%.2f, %.2f)",
                entity_id, position.x, position.y);

rogue_log_error("Failed to load texture: %s", filename);

// Custom log categories
rogue_log_category_set_level(LOG_CATEGORY_COMBAT, ROGUE_LOG_LEVEL_TRACE);
rogue_log_combat("Player attacked for %d damage", damage);
```

### Performance Profiling
```c
// Profile code sections
ROGUE_PROFILE_BEGIN("entity_update");
for (size_t i = 0; i < entity_count; i++) {
    rogue_entity_update(entities[i], delta_time);
}
ROGUE_PROFILE_END();

// Get profiling results
RogueProfileResult result;
rogue_profile_get_result("entity_update", &result);
printf("Entity update: %.2f ms average\n", result.average_time_ms);
```

### Memory Debugging
```c
// Enable memory tracking
rogue_memory_enable_tracking(true);

// Check for leaks
size_t leak_count = rogue_memory_get_leak_count();
if (leak_count > 0) {
    printf("Memory leaks detected: %zu\n", leak_count);

    // Get leak details
    RogueMemoryLeak* leaks = rogue_memory_get_leaks();
    for (size_t i = 0; i < leak_count; i++) {
        printf("Leak at %s:%d, size %zu bytes\n",
               leaks[i].file, leaks[i].line, leaks[i].size);
    }
}
```

## 📖 Best Practices

### API Usage Patterns

#### Resource Management
- Always check return values for errors
- Use RAII pattern when possible
- Clean up resources in reverse order of creation
- Handle out-of-memory conditions gracefully

#### Thread Safety
- Know which functions are thread-safe
- Use appropriate synchronization primitives
- Avoid shared mutable state when possible
- Document threading requirements

#### Error Handling
- Check error codes from all API calls
- Provide meaningful error messages
- Handle edge cases and invalid inputs
- Log errors with sufficient context

### Performance Optimization

#### Memory Efficiency
- Reuse objects from pools when possible
- Pre-allocate containers to avoid reallocation
- Use stack allocation for small, temporary objects
- Profile memory usage regularly

#### CPU Optimization
- Batch similar operations together
- Use appropriate data structures for access patterns
- Minimize cache misses with data locality
- Profile hot paths and optimize bottlenecks

#### I/O Optimization
- Load assets asynchronously when possible
- Use streaming for large data sets
- Cache frequently accessed data
- Compress data to reduce I/O

## 🔗 Related Documentation

- **[Developer Hub](../developers/index.md)** - Development setup and workflow
- **[Contributing Guide](../developers/contributing.md)** - Code style and contribution process
- **[Architecture Overview](../developers/architecture.md)** - System design and organization
- **[Debug Overlay Guide](../developers/debug_overlay.md)** - Development tools usage

---

## 📚 Complete API Reference

This hub provides an overview of the API structure and common patterns. For detailed function documentation, see the generated Doxygen documentation or explore the specific system guides linked above.

**Need help with the API?** Check our [Discord community](https://discord.gg/roguelike) or search the [developer forum](https://forum.roguelike.com/developers) for examples and discussions.

*API documentation is automatically generated from code comments and updated with each release. Last updated: September 2025*
