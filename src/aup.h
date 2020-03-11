#ifndef _AUP_HEADER_H
#define _AUP_HEADER_H
#pragma once

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stddef.h>
#include <stdint.h>
#if defined(_MSC_VER) && (_MSC_VER < 1800)
#define bool    int
#define false   0
#define true    1
#else
#include <stdbool.h>
#endif

#if (defined(DEBUG) || defined(_DEBUG)) && !(defined(NDEBUG)  || defined(_NDEBUG))
#define AUP_DEBUG
#endif

#if defined(__i386) || defined(__i386__) || defined(_M_IX86)
#define AUP_X86
#elif defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
#define AUP_X64
#elif defined(__arm__) || defined(__arm) || defined(__ARM__) || defined(__ARM)
#define AUP_ARM
#else
#error "This architecture is not supported!"
#endif

#if defined(_WIN32) && !defined(_XBOX_VER)
#define AUP_WIN32
#elif defined(__linux__)
#define AUP_LINUX
#elif defined(__MACH__) && defined(__APPLE__)
#define AUP_OSX
#endif

#endif
