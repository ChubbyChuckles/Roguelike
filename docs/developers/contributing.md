@page contributing_guide Contributing Guide

# Contributing to the Roguelike Engine

Thank you for your interest in contributing to the Roguelike Engine! This guide provides comprehensive information for contributors of all experience levels, from first-time contributors to experienced developers.

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Code Style Guidelines](#code-style-guidelines)
- [Pull Request Process](#pull-request-process)
- [Issue Reporting](#issue-reporting)
- [Testing Guidelines](#testing-guidelines)
- [Documentation Standards](#documentation-standards)
- [Community Guidelines](#community-guidelines)

## 🤝 Code of Conduct

### Our Commitment
We are committed to providing a welcoming, inclusive, and harassment-free environment for all contributors, regardless of:
- Age, body size, disability, ethnicity, gender identity and expression
- Level of experience, nationality, personal appearance, race, religion
- Sexual identity and orientation
- Technological choices

### Expected Behavior
- **Be respectful**: Use welcoming and inclusive language
- **Be collaborative**: Work together to resolve conflicts and challenges
- **Be patient**: Remember that people have different backgrounds and experiences
- **Be constructive**: Focus on what we can build together
- **Show empathy**: Consider how your words and actions affect others

### Unacceptable Behavior
- Harassment, intimidation, or discrimination
- Personal attacks or derogatory comments
- Trolling or inflammatory remarks
- Publishing others' private information
- Any other conduct that could reasonably be considered inappropriate

### Reporting Violations
If you experience or witness unacceptable behavior, please report it by:
- Contacting the project maintainers privately
- Using the [anonymous reporting form](https://forms.gle/report-conduct-violation)
- Reaching out to the community moderators on Discord

## 🚀 Getting Started

### Prerequisites
Before contributing, ensure you have:
- Git and GitHub account
- Development environment set up (see [Developer Hub](index.md))
- Basic understanding of C programming
- Familiarity with CMake build system

### First Contributions
For your first contribution, we recommend:
1. **Read the documentation**: Start with the [Developer Hub](index.md)
2. **Set up your environment**: Follow the [setup guide](setup.md)
3. **Build the project**: Complete the [building tutorial](building.md)
4. **Run tests**: Validate with [testing guide](testing.md)
5. **Find an issue**: Look for "good first issue" or "help wanted" labels

### Types of Contributions
We welcome contributions in many forms:
- **Code**: Bug fixes, new features, performance improvements
- **Documentation**: Guides, tutorials, API documentation
- **Testing**: Unit tests, integration tests, performance tests
- **Design**: UI/UX improvements, asset creation
- **Research**: Algorithm improvements, technical analysis
- **Community**: Helping other contributors, moderating discussions

## 🔄 Development Workflow

### Git Workflow

#### Branch Strategy
```
Repository Structure:
├── main: Production-ready code, always stable
├── develop: Integration branch for new features
├── feature/*: New features and major changes
├── bugfix/*: Bug fixes and minor improvements
├── hotfix/*: Critical fixes for production
└── release/*: Release preparation and stabilization
```

#### Working with Branches
```
Standard Workflow:
1. Fork the repository on GitHub
2. Clone your fork locally
3. Create a feature branch from develop
   ├── git checkout develop
   ├── git pull origin develop
   ├── git checkout -b feature/your-feature-name
4. Make your changes with regular commits
5. Push to your fork
6. Create a pull request to the main repository
```

### Commit Standards

#### Commit Message Format
```
Type: Brief description of changes

Detailed explanation of what was changed and why.
Include any relevant issue numbers or context.

Type: feat, fix, docs, style, refactor, test, chore
Scope: Optional category in parentheses
```

#### Examples
```
feat: Add real-time debug overlay system

Implement comprehensive debug overlay with panels for system
monitoring, entity inspection, and live asset editing. Includes
performance metrics, map editing tools, and content validation.

Closes #123
```

```
fix: Resolve memory leak in audio system

Fixed memory leak in audio buffer allocation by properly
freeing resources in cleanup path. Added unit test to prevent
regression.

Fixes #456
```

#### Commit Best Practices
- **Atomic commits**: Each commit should contain one logical change
- **Clear messages**: Explain what and why, not just how
- **Reference issues**: Link to relevant GitHub issues
- **Test before commit**: Ensure all tests pass
- **Frequent commits**: Commit early and often

## 📝 Code Style Guidelines

### C Language Standards

#### Formatting
```
Basic Rules:
├── Use 4 spaces for indentation (no tabs)
├── Maximum line length: 80 characters
├── K&R brace style for functions and control structures
├── One space after commas, no space before
├── One blank line between functions
└── Two blank lines between source files sections
```

#### Example
```c
/**
 * @brief Calculate damage with modifiers
 * @param base_damage The base damage value
 * @param modifiers Array of damage modifiers
 * @param count Number of modifiers
 * @return Final damage value
 */
int rogue_calculate_damage(int base_damage,
                          const float* modifiers,
                          int count) {
    float multiplier = 1.0f;

    for (int i = 0; i < count; i++) {
        multiplier *= modifiers[i];
    }

    return (int)(base_damage * multiplier);
}
```

### Naming Conventions

#### Functions and Variables
```
Functions: snake_case with descriptive names
├── rogue_combat_calculate_damage()
├── rogue_entity_get_position()
└── rogue_audio_play_sound()

Variables: snake_case with context
├── player_health, enemy_count
├── damage_multiplier, spawn_timer
└── config_file_path, texture_handle

Constants: SCREAMING_SNAKE_CASE
├── MAX_ENTITIES, DEFAULT_VOLUME
├── FRAME_RATE_TARGET, MEMORY_POOL_SIZE
└── CONFIG_FILE_PATH, LOG_LEVEL_DEBUG
```

#### Types and Structures
```
Structures: PascalCase with Rogue prefix
├── RogueCombatState, RogueEntity
├── RogueAudioSystem, RogueConfig
└── RogueDebugOverlay, RogueSkillData

Enums: PascalCase with descriptive names
├── RogueDamageType { PHYSICAL, FIRE, ICE }
├── RogueEntityState { IDLE, MOVING, ATTACKING }
└── RogueLogLevel { DEBUG, INFO, WARN, ERROR }
```

### Documentation Standards

#### Function Documentation
```c
/**
 * @brief Brief description of function purpose
 *
 * Detailed description of what the function does,
 * including algorithm details, edge cases, and usage notes.
 *
 * @param[in] input_param Description of input parameter
 * @param[out] output_param Description of output parameter
 * @param[in,out] buffer Working buffer for operation
 *
 * @return Description of return value and possible errors
 * @retval 0 Success
 * @retval -1 Invalid parameter
 * @retval -2 Out of memory
 *
 * @note Performance considerations or important warnings
 * @warning This function may block for extended periods
 * @see RelatedFunction(), RelatedStructure
 *
 * @code{.c}
 * // Example usage
 * int result = example_function(param);
 * if (result != 0) {
 *     handle_error(result);
 * }
 * @endcode
 */
```

#### File Headers
```c
/**
 * @file combat_system.c
 * @brief Core combat mechanics and damage calculation
 *
 * This module implements the frame-perfect combat system including
 * attack state machines, damage calculation, and hit detection.
 * The system is designed for deterministic replay and precise
 * timing control.
 *
 * Key Components:
 * - Attack state management
 * - Damage calculation pipeline
 * - Hit detection and collision
 * - Combat timing and animation sync
 *
 * @author Development Team
 * @date 2025-01-15
 * @version 1.0.0
 *
 * @defgroup CombatSystem Combat System
 * @{
 */
```

### Error Handling

#### Error Codes and Messages
```c
/**
 * Combat system error codes
 */
typedef enum {
    ROGUE_COMBAT_SUCCESS = 0,
    ROGUE_COMBAT_INVALID_ENTITY = -1,
    ROGUE_COMBAT_OUT_OF_RANGE = -2,
    ROGUE_COMBAT_COOLDOWN_ACTIVE = -3,
    ROGUE_COMBAT_INSUFFICIENT_RESOURCE = -4
} RogueCombatError;

/**
 * Get human-readable error message
 */
const char* rogue_combat_error_string(RogueCombatError error) {
    switch (error) {
        case ROGUE_COMBAT_SUCCESS:
            return "Operation completed successfully";
        case ROGUE_COMBAT_INVALID_ENTITY:
            return "Invalid or null entity provided";
        case ROGUE_COMBAT_OUT_OF_RANGE:
            return "Target is out of attack range";
        case ROGUE_COMBAT_COOLDOWN_ACTIVE:
            return "Attack is on cooldown";
        case ROGUE_COMBAT_INSUFFICIENT_RESOURCE:
            return "Insufficient resources for attack";
        default:
            return "Unknown error occurred";
    }
}
```

## 🔄 Pull Request Process

### Creating a Pull Request

#### Before Submitting
```
Preparation Checklist:
□ Code compiles without warnings
□ All tests pass (unit, integration, performance)
□ Documentation updated for new features
□ Code follows style guidelines
□ Commit messages are clear and descriptive
□ Branch is up-to-date with develop
□ No merge conflicts exist
□ Related issues are referenced
```

#### PR Template
```
Pull Request Title: [TYPE] Brief description of changes

## Description
Detailed explanation of what was implemented and why.

## Changes Made
- List of specific changes
- Files modified
- New features added
- Bugs fixed

## Testing
- Unit tests added/modified
- Integration tests verified
- Performance impact assessed
- Manual testing completed

## Screenshots (if applicable)
- Before/after comparisons
- UI changes
- New features in action

## Related Issues
- Closes #123
- Fixes #456
- Related to #789

## Checklist
- [x] Code compiles without errors
- [x] Tests pass
- [x] Documentation updated
- [x] Style guidelines followed
- [x] Commit messages clear
```

### Review Process

#### Automated Checks
```
CI Pipeline:
├── Build verification (all platforms)
├── Test execution (unit, integration)
├── Static analysis (clang-tidy, cppcheck)
├── Code coverage analysis
├── Documentation generation
└── Performance regression detection
```

#### Peer Review
```
Review Criteria:
├── Code correctness and functionality
├── Adherence to style guidelines
├── Test coverage and quality
├── Documentation completeness
├── Performance implications
├── Security considerations
└── Maintainability and readability
```

#### Review Guidelines for Reviewers
```
Constructive Feedback:
├── Explain reasoning for requested changes
├── Suggest alternatives when possible
├── Acknowledge good practices
├── Focus on code, not author
├── Be patient and respectful
└── Recognize effort and improvement
```

### Merging
```
Merge Requirements:
├── All automated checks pass
├── At least one approving review
├── No outstanding critical issues
├── Branch up-to-date with target
├── Clean merge (no conflicts)
└── Appropriate merge strategy (squash, merge, rebase)
```

## 🐛 Issue Reporting

### Bug Reports

#### Bug Report Template
```
Bug Report: [Clear, descriptive title]

## Description
Brief description of the bug and its impact.

## Steps to Reproduce
1. Go to '...'
2. Click on '...'
3. Scroll down to '...'
4. See error

## Expected Behavior
What should happen when following the steps above.

## Actual Behavior
What actually happens, including any error messages.

## Environment
- OS: [e.g., Windows 10, Ubuntu 20.04]
- Version: [e.g., commit hash, version number]
- Build: [Debug/Release]
- Hardware: [CPU, RAM, GPU if relevant]

## Additional Context
- Screenshots or videos
- Log files
- Save files that reproduce the issue
- Any workarounds discovered

## Related Issues
- Links to related issues or discussions
- Similar reports from other users
```

### Feature Requests

#### Feature Request Template
```
Feature Request: [Clear, descriptive title]

## Problem Statement
What problem are you trying to solve? What is the current limitation?

## Proposed Solution
Describe your proposed solution in detail.

## Alternative Solutions
Describe any alternative solutions you've considered.

## Use Cases
- Primary use case and user story
- Secondary use cases
- Edge cases to consider

## Implementation Notes
- Technical requirements
- Dependencies or prerequisites
- Potential challenges
- Performance considerations

## Mockups or Examples
- UI mockups if applicable
- Code examples
- Similar features in other projects
```

### Issue Management

#### Issue Labels
```
Priority:
├── critical: Blocks development or major functionality
├── high: Important but not blocking
├── medium: Should be addressed
├── low: Nice to have, future consideration

Type:
├── bug: Something isn't working
├── feature: New feature request
├── enhancement: Improvement to existing feature
├── documentation: Documentation issue
├── question: Further information needed
├── wontfix: Will not be implemented

Status:
├── good first issue: Suitable for new contributors
├── help wanted: Community contribution welcome
├── in progress: Someone is working on this
├── blocked: Waiting on other work
├── duplicate: Already reported elsewhere
```

## 🧪 Testing Guidelines

### Unit Testing

#### Test Structure
```c
/**
 * @brief Test combat damage calculation
 */
void test_combat_damage_calculation(void) {
    // Arrange
    RogueCombatState* combat = rogue_combat_create();
    int base_damage = 100;
    float multiplier = 1.5f;

    // Act
    int result = rogue_combat_calculate_damage(combat, base_damage, multiplier);

    // Assert
    TEST_ASSERT_EQUAL(150, result);
    TEST_ASSERT_TRUE(result > base_damage);

    // Cleanup
    rogue_combat_destroy(combat);
}
```

#### Test Coverage Requirements
```
Coverage Targets:
├── Statement coverage: >90%
├── Branch coverage: >80%
├── Function coverage: >95%
├── Line coverage: >85%

Critical Path Coverage:
├── Error handling paths
├── Edge cases and boundary conditions
├── Memory allocation/deallocation
├── Resource cleanup
└── Performance-critical code
```

### Integration Testing

#### End-to-End Test Example
```c
/**
 * @brief Test complete combat encounter
 */
void test_combat_encounter_flow(void) {
    // Setup game state
    RogueGameState* game = create_test_game_state();

    // Create entities
    RogueEntity* player = create_test_player(game);
    RogueEntity* enemy = create_test_enemy(game);

    // Execute combat sequence
    rogue_combat_initiate_attack(player, enemy);
    rogue_game_update(game, 100); // 100ms tick

    // Verify results
    TEST_ASSERT_TRUE(rogue_entity_get_health(enemy) < 100);
    TEST_ASSERT_EQUAL(COMBAT_STATE_ATTACKING,
                     rogue_entity_get_combat_state(player));

    // Cleanup
    destroy_test_game_state(game);
}
```

### Performance Testing

#### Benchmark Template
```c
/**
 * @brief Benchmark combat system performance
 */
void benchmark_combat_system(void) {
    const int iterations = 10000;
    RogueCombatState* combat = rogue_combat_create();

    // Warm-up
    for (int i = 0; i < 100; i++) {
        rogue_combat_update(combat, 16); // 16ms frame
    }

    // Benchmark
    uint64_t start_time = get_current_time_ns();
    for (int i = 0; i < iterations; i++) {
        rogue_combat_update(combat, 16);
    }
    uint64_t end_time = get_current_time_ns();

    // Calculate and report
    double avg_time = (end_time - start_time) / (double)iterations;
    printf("Average combat update time: %.2f ns\n", avg_time);

    TEST_ASSERT_TRUE(avg_time < 1000000); // Less than 1ms

    rogue_combat_destroy(combat);
}
```

## 📚 Documentation Standards

### API Documentation

#### Function Documentation Template
```c
/**
 * @brief [One-line description of function purpose]
 *
 * [Detailed description of what the function does, including:
 * - Algorithm or logic explanation
 * - Parameter interactions
 * - Side effects
 * - Thread safety considerations
 * - Performance characteristics]
 *
 * @param[in] input Description of input parameter with valid ranges
 * @param[out] output Description of output parameter and expected values
 * @param[in,out] buffer Description of modified parameter
 *
 * @return [Description of return value]
 * @retval 0 Success condition with explanation
 * @retval -1 Specific error condition
 *
 * @note [Important notes about usage, limitations, or gotchas]
 * @warning [Critical warnings about misuse or dangerous conditions]
 * @see [Related functions, structures, or concepts]
 * @since [Version when this function was added]
 *
 * @code{.c}
 * // Example usage with realistic values
 * int result = example_function(param1, &output);
 * if (result == 0) {
 *     // Success handling
 * } else {
 *     // Error handling
 * }
 * @endcode
 */
```

### Module Documentation

#### Module Overview
```c
/**
 * @defgroup CombatSystem Combat System
 * @brief Frame-perfect combat mechanics and damage calculation
 *
 * The combat system provides deterministic, timing-based combat with
 * precise control over attack windows, defensive mechanics, and
 * damage calculation. All combat is designed for perfect replay
 * and competitive fairness.
 *
 * ## Key Features
 * - Frame-perfect attack timing
 * - Comprehensive defensive options
 * - Deterministic damage calculation
 * - Performance-optimized collision detection
 *
 * ## Architecture
 * The system is built around several core components:
 * - Attack state machines for timing control
 * - Collision detection with pixel-perfect accuracy
 * - Damage calculation with modifier stacking
 * - Defensive mechanics (parry, dodge, block)
 *
 * ## Usage Example
 * @code{.c}
 * // Initialize combat system
 * RogueCombatState* combat = rogue_combat_create();
 *
 * // Execute attack
 * rogue_combat_initiate_attack(player, target);
 *
 * // Update timing
 * rogue_combat_update(combat, delta_time_ms);
 * @endcode
 *
 * ## Performance Notes
 * - Combat updates are optimized for <1ms execution
 * - Memory usage scales with active combatants
 * - Collision detection uses spatial partitioning
 *
 * @{
 */

/* Module functions and types go here */

/** @} */ // End of CombatSystem group
```

## 🤝 Community Guidelines

### Communication Standards

#### Discord Guidelines
```
Channel Usage:
├── #general: General discussion and questions
├── #development: Technical development topics
├── #bug-reports: Bug reporting and discussion
├── #feature-requests: Feature suggestions and discussion
├── #showcase: Sharing projects and achievements
└── #off-topic: Non-technical discussion

Posting Guidelines:
├── Use appropriate channels for topics
├── Provide context and background
├── Be respectful and constructive
├── Use code blocks for code snippets
├── Search before asking (check pins)
└── Keep discussions on-topic
```

#### Forum Guidelines
```
Posting Standards:
├── Use descriptive titles
├── Provide detailed information
├── Include steps to reproduce issues
├── Search existing threads first
├── Use appropriate tags and categories
└── Follow up on your own threads

Discussion Etiquette:
├── Stay on topic
├── Be patient with responses
├── Acknowledge helpful answers
├── Correct misinformation politely
├── Avoid thread necromancy
└── Use quote feature for context
```

### Recognition and Attribution

#### Contribution Recognition
```
Recognition System:
├── GitHub contributor statistics
├── Discord role progression
├── Forum reputation system
├── Monthly contributor spotlights
└── Annual contributor awards

Attribution Standards:
├── Code comments include author information
├── Documentation credits contributors
├── Changelog entries for significant contributions
└── Community showcase for major projects
```

### Conflict Resolution

#### Handling Disagreements
```
Resolution Process:
1. Acknowledge the disagreement respectfully
2. Focus on technical merits, not personal preferences
3. Provide evidence or data to support positions
4. Seek compromise or alternative solutions
5. Escalate to maintainers if needed
6. Accept majority or maintainer decisions
```

#### Mediation Process
```
If conflicts arise:
├── Attempt private resolution first
├── Involve neutral third parties
├── Document the disagreement
├── Follow project decision-making process
└── Maintain professional conduct throughout
```

---

## 🎯 Getting Help

### Support Resources
- **Documentation**: Comprehensive guides and API references
- **Discord Community**: Real-time help and discussion
- **GitHub Issues**: Bug reports and feature requests
- **Forum**: In-depth technical discussions
- **Wiki**: Community-maintained knowledge base

### Asking for Help Effectively
```
Effective Help Requests:
├── Provide clear problem description
├── Include relevant code snippets
├── Specify environment and versions
├── Describe attempted solutions
├── Be patient and responsive
└── Show appreciation for help received
```

---

Thank you for contributing to the Roguelike Engine! Your contributions help make this project better for everyone in the community.

**Ready to contribute?** Start by exploring our [Developer Hub](index.md) and finding your first issue to work on.

*This contributing guide is living documentation. Please suggest improvements or clarifications as you encounter areas that could be clearer.*
