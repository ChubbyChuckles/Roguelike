/*
MIT License

Copyright (c) 2025 ChubbyChuckles

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation bool rogue_integration_pause_system(uint32_t
system_id); bool rogue_integration_resume_system(uint32_t system_id);les (the "Software"), to deal
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

#ifndef ROGUE_INTEGRATION_MANAGER_H
#define ROGUE_INTEGRATION_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Maximum number of systems that can be registered */
#define ROGUE_MAX_SYSTEMS 32

    /* System priority levels (Phase 0.1.3) */
    typedef enum
    {
        ROGUE_SYSTEM_PRIORITY_CRITICAL = 0,  /* Must never fail (Core gameplay) */
        ROGUE_SYSTEM_PRIORITY_IMPORTANT = 1, /* Can degrade gracefully */
        ROGUE_SYSTEM_PRIORITY_OPTIONAL = 2,  /* Can be disabled */
        ROGUE_SYSTEM_PRIORITY_COUNT
    } RogueSystemPriority;

    /* System type classification (Phase 0.1.2) */
    typedef enum
    {
        ROGUE_SYSTEM_TYPE_CORE = 0,           /* AI, Combat, Physics */
        ROGUE_SYSTEM_TYPE_CONTENT = 1,        /* Loot, Crafting, Skills */
        ROGUE_SYSTEM_TYPE_UI = 2,             /* HUD, Menus, Panels */
        ROGUE_SYSTEM_TYPE_INFRASTRUCTURE = 3, /* Persistence, World Gen */
        ROGUE_SYSTEM_TYPE_COUNT
    } RogueSystemType;

    /* System lifecycle states (Phase 0.3.3) */
    typedef enum
    {
        ROGUE_SYSTEM_STATE_UNINITIALIZED = 0,
        ROGUE_SYSTEM_STATE_INITIALIZING = 1,
        ROGUE_SYSTEM_STATE_RUNNING = 2,
        ROGUE_SYSTEM_STATE_PAUSED = 3,
        ROGUE_SYSTEM_STATE_SHUTDOWN = 4,
        ROGUE_SYSTEM_STATE_FAILED = 5,
        ROGUE_SYSTEM_STATE_COUNT
    } RogueSystemState;

    /* System capability flags (Phase 0.1.4) */
    typedef enum
    {
        ROGUE_SYSTEM_CAP_PROVIDES_ENTITIES = 0x01,  /* Creates/manages entities */
        ROGUE_SYSTEM_CAP_CONSUMES_EVENTS = 0x02,    /* Subscribes to events */
        ROGUE_SYSTEM_CAP_PRODUCES_EVENTS = 0x04,    /* Publishes events */
        ROGUE_SYSTEM_CAP_REQUIRES_RENDERING = 0x08, /* Needs render pass */
        ROGUE_SYSTEM_CAP_REQUIRES_UPDATE = 0x10,    /* Needs update tick */
        ROGUE_SYSTEM_CAP_CONFIGURABLE = 0x20,       /* Has configuration */
        ROGUE_SYSTEM_CAP_SERIALIZABLE = 0x40,       /* Can save/load state */
        ROGUE_SYSTEM_CAP_HOT_RELOADABLE = 0x80      /* Supports hot-reload */
    } RogueSystemCapability;

    /* System resource usage patterns (Phase 0.1.6) */
    typedef struct
    {
        uint32_t cpu_usage_percent;  /* 0-100 estimated CPU usage */
        uint32_t memory_usage_kb;    /* Estimated memory usage in KB */
        uint32_t io_ops_per_frame;   /* I/O operations per frame */
        uint32_t network_kb_per_sec; /* Network usage in KB/s */
    } RogueSystemResourceUsage;

    /* System vitality indicators (Phase 0.3.4) */
    typedef struct
    {
        uint32_t uptime_seconds;    /* Time since last restart */
        uint32_t error_count;       /* Number of recoverable errors */
        uint32_t restart_count;     /* Number of restarts */
        double last_update_time_ms; /* Last successful update */
        bool is_responsive;         /* Responding to health checks */
    } RogueSystemHealth;

    /* Forward declarations */
    typedef struct RogueSystemInterface RogueSystemInterface;
    typedef struct RogueSystemDescriptor RogueSystemDescriptor;

    /* System interface contract (Phase 0.5.1) */
    typedef struct RogueSystemInterface
    {
        /* Mandatory methods */
        bool (*init)(void* system_data);
        void (*update)(void* system_data, double dt_ms);
        void (*shutdown)(void* system_data);
        void* (*get_state)(void* system_data);

        /* Optional extensions */
        bool (*set_config)(void* system_data, const void* config);
        bool (*serialize)(void* system_data, void* buffer, size_t* size);
        bool (*deserialize)(void* system_data, const void* buffer, size_t size);
        void (*debug_info)(void* system_data, char* buffer, size_t size);
    } RogueSystemInterface;

    /* System descriptor (Phase 0.1.1) */
    typedef struct RogueSystemDescriptor
    {
        uint32_t system_id;                 /* Unique system identifier */
        const char* name;                   /* Human-readable name */
        const char* version;                /* Version string */
        RogueSystemType type;               /* System classification */
        RogueSystemPriority priority;       /* Priority level */
        uint32_t capabilities;              /* Bitfield of capabilities */
        RogueSystemResourceUsage resources; /* Resource usage patterns */

        /* Dependencies (Phase 0.2) */
        uint32_t hard_dependencies[8]; /* Systems that must be initialized first */
        uint32_t soft_dependencies[8]; /* Systems that improve functionality */
        uint8_t hard_dep_count;
        uint8_t soft_dep_count;

        /* Interface and data */
        RogueSystemInterface interface; /* Function pointers */
        void* system_data;              /* System-specific data */
    } RogueSystemDescriptor;

    /* System registry entry */
    typedef struct
    {
        RogueSystemDescriptor descriptor;
        RogueSystemState current_state;
        RogueSystemHealth health;
        double last_restart_time_ms;
        uint32_t restart_backoff_ms;
    } RogueSystemEntry;

    /* Integration Manager (Phase 0.3) */
    typedef struct
    {
        RogueSystemEntry systems[ROGUE_MAX_SYSTEMS];
        uint32_t system_count;
        uint32_t next_system_id;
        bool initialization_complete;
        double manager_uptime_ms;

        /* Dependency graph (Phase 0.2) */
        uint32_t initialization_order[ROGUE_MAX_SYSTEMS];
        uint32_t initialization_groups[ROGUE_MAX_SYSTEMS]; /* Systems that can init in parallel */
        uint8_t group_count;

        /* Performance monitoring */
        double total_update_time_ms;
        double max_update_time_ms;
        uint32_t update_call_count;
    } RogueIntegrationManager;

    /* Global integration manager instance */
    extern RogueIntegrationManager g_integration_manager;

    /* Core API */
    bool rogue_integration_manager_init(void);
    void rogue_integration_manager_shutdown(void);
    void rogue_integration_manager_update(double dt_ms);

    /* System registration (Phase 0.3.2) */
    uint32_t rogue_integration_register_system(const RogueSystemDescriptor* descriptor);
    bool rogue_integration_unregister_system(uint32_t system_id);
    RogueSystemEntry* rogue_integration_get_system(uint32_t system_id);
    RogueSystemEntry* rogue_integration_find_system_by_name(const char* name);

    /* System lifecycle control (Phase 0.3.3) */
    bool rogue_integration_initialize_system(uint32_t system_id);
    bool rogue_integration_shutdown_system(uint32_t system_id);
    bool rogue_integration_restart_system(uint32_t system_id);
    bool rogue_integration_pause_system(uint32_t system_id);
    bool rogue_integration_resume_system(uint32_t system_id);

    /* Dependency management (Phase 0.2) */
    bool rogue_integration_build_dependency_graph(void);
    bool rogue_integration_validate_dependencies(void);
    bool rogue_integration_get_initialization_order(uint32_t* order_buffer, size_t buffer_size);

    /* Health monitoring (Phase 0.3.4) */
    void rogue_integration_update_system_health(uint32_t system_id);
    bool rogue_integration_is_system_healthy(uint32_t system_id);
    void rogue_integration_get_health_report(char* buffer, size_t buffer_size);

    /* System taxonomy utilities (Phase 0.1) */
    const char* rogue_integration_system_type_name(RogueSystemType type);
    const char* rogue_integration_system_priority_name(RogueSystemPriority priority);
    const char* rogue_integration_system_state_name(RogueSystemState state);
    bool rogue_integration_has_capability(uint32_t system_id, RogueSystemCapability capability);

    /* Performance monitoring (Phase 0.7) */
    double rogue_integration_get_average_update_time_ms(void);
    double rogue_integration_get_max_update_time_ms(void);
    void rogue_integration_reset_performance_counters(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_INTEGRATION_MANAGER_H */
