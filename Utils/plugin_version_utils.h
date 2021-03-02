#pragma once

#include "local-version.h"
#include "git_commit_version.h"

#define MAKE_STRING0(s) #s
#define MAKE_STRING(s) MAKE_STRING0(s)

// #if sizeof void* == 8
// #define BITNESS_STRING "x86_64"
// #elif sizeof void* == 4
// #define BITNESS_STRING "x86_32"
// #else
// #error Unknown bitness
// #endif



#ifdef WIN64
#define BITNESS_STRING "x86_64"
#else
#define BITNESS_STRING "x86_32"
#endif

#if defined BUILD_MODE_DEBUG
#define DEBUG_STRING "debug"
#elif defined BUILD_MODE_TEST
#define DEBUG_STRING "test"
#elif defined BUILD_MODE_RELEASE
#define DEBUG_STRING "release"
#else
#error Unknown build mode
#endif


#define TARGET_RAINMETER_VERSION_1 4
#define TARGET_RAINMETER_VERSION_2 2
#define TARGET_RAINMETER_VERSION_3 0
#define TARGET_RAINMETER_VERSION_4 3111

#define TARGET_RAINMETER_VERSION TARGET_RAINMETER_VERSION_1,TARGET_RAINMETER_VERSION_2,TARGET_RAINMETER_VERSION_3,TARGET_RAINMETER_VERSION_4
#define TARGET_RAINMETER_VERSION_STRING_WITHOUT_BITNESS \
	MAKE_STRING(TARGET_RAINMETER_VERSION_1) "." \
	MAKE_STRING(TARGET_RAINMETER_VERSION_2) "." \
	MAKE_STRING(TARGET_RAINMETER_VERSION_3) "." \
	MAKE_STRING(TARGET_RAINMETER_VERSION_4)

#define TARGET_RAINMETER_VERSION_STRING TARGET_RAINMETER_VERSION_STRING_WITHOUT_BITNESS " (" BITNESS_STRING ")"

#if (PLUGIN_VERSION_COMPONENTS_COUNT <= 2)

#define PLUGIN_VERSION PLUGIN_VERSION_COMPONENT_1,PLUGIN_VERSION_COMPONENT_2
#define PLUGIN_VERSION_STRING \
	MAKE_STRING(PLUGIN_VERSION_COMPONENT_1) "." \
	MAKE_STRING(PLUGIN_VERSION_COMPONENT_2)

#elif (PLUGIN_VERSION_COMPONENTS_COUNT == 3)

#define PLUGIN_VERSION PLUGIN_VERSION_COMPONENT_1,PLUGIN_VERSION_COMPONENT_2,PLUGIN_VERSION_COMPONENT_3
#define PLUGIN_VERSION_STRING \
	MAKE_STRING(PLUGIN_VERSION_COMPONENT_1) "." \
	MAKE_STRING(PLUGIN_VERSION_COMPONENT_2) "." \
	MAKE_STRING(PLUGIN_VERSION_COMPONENT_3)

#else

#define PLUGIN_VERSION PLUGIN_VERSION_COMPONENT_1,PLUGIN_VERSION_COMPONENT_2,PLUGIN_VERSION_COMPONENT_3,PLUGIN_VERSION_COMPONENT_4
#define PLUGIN_VERSION_STRING \
	MAKE_STRING(PLUGIN_VERSION_COMPONENT_1) "." \
	MAKE_STRING(PLUGIN_VERSION_COMPONENT_2) "." \
	MAKE_STRING(PLUGIN_VERSION_COMPONENT_3) "." \
	MAKE_STRING(PLUGIN_VERSION_COMPONENT_4)

#endif

#ifdef GIT_DIRTY_SUFFIX
#define PLUGIN_FULL_VERSION_STRING PLUGIN_VERSION_STRING "-" BITNESS_STRING "-" DEBUG_STRING "-" MAKE_STRING(GIT_CUR_COMMIT_HASH) MAKE_STRING(GIT_DIRTY_SUFFIX)
#else
#define PLUGIN_FULL_VERSION_STRING PLUGIN_VERSION_STRING "-" BITNESS_STRING "-" DEBUG_STRING "-" MAKE_STRING(GIT_CUR_COMMIT_HASH)
#endif

#if (PLUGIN_NAME_INCLUDE_VERSIONS + 0 == 0)

#define PLUGIN_ORIGINAL_FILE_NAME PLUGIN_NAME

#elif (PLUGIN_NAME_INCLUDE_VERSIONS + 0 == 1)

#define PLUGIN_ORIGINAL_FILE_NAME PLUGIN_NAME "-v" \
	MAKE_STRING(PLUGIN_VERSION_COMPONENT_1)

#elif (PLUGIN_NAME_INCLUDE_VERSIONS + 0 == 2)

#define PLUGIN_ORIGINAL_FILE_NAME PLUGIN_NAME "-v" \
	MAKE_STRING(PLUGIN_VERSION_COMPONENT_1) "." \
	MAKE_STRING(PLUGIN_VERSION_COMPONENT_2)

#else

#error PLUGIN_NAME_INCLUDE_VERSIONS can only be 0, 1, 2

#endif