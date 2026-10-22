#pragma once

// -----------------------
// DLL export / import
// -----------------------
// FlexCore is built as a DLL so the editor, the runtime and game modules
// all share ONE copy of the engine (one set of registries, managers, statics).
#ifdef FLEX_CORE_EXPORTS
	#define FLEX_API __declspec(dllexport)
#else
	#define FLEX_API __declspec(dllimport)
#endif

// C4251: "class needs to have dll-interface" for STL members of exported classes.
// Safe here because every module is built with the same compiler and CRT (Build/*.props).
#pragma warning(disable: 4251)

// -----------------------
// Build configuration
// -----------------------
// Set per configuration in Build/FlexCommon.props.
#ifndef FLEX_DEBUG
	#define FLEX_DEBUG 0
#endif
#ifndef FLEX_DEVELOPMENT
	#define FLEX_DEVELOPMENT 0
#endif
#ifndef FLEX_SHIPPING
	#define FLEX_SHIPPING 0
#endif
#ifndef FLEX_DEBUG_TOOLS
	#define FLEX_DEBUG_TOOLS 0
#endif

static_assert(FLEX_DEBUG + FLEX_DEVELOPMENT + FLEX_SHIPPING == 1, "Exactly one of FLEX_DEBUG, FLEX_DEVELOPMENT or FLEX_SHIPPING must be set.");

#if FLEX_DEBUG
	#define FLEX_BUILD_CONFIG_NAME "Debug"
#elif FLEX_DEVELOPMENT
	#define FLEX_BUILD_CONFIG_NAME "Development"
#else
	#define FLEX_BUILD_CONFIG_NAME "Shipping"
#endif

#define FLEX_VERSION_STRING "0.1.0"
