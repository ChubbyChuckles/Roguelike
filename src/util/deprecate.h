#ifndef ROGUE_UTIL_DEPRECATE_H
#define ROGUE_UTIL_DEPRECATE_H
#if defined(_MSC_VER)
#define ROGUE_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(__GNUC__) || defined(__clang__)
#define ROGUE_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
#define ROGUE_DEPRECATED(msg)
#endif
#endif
