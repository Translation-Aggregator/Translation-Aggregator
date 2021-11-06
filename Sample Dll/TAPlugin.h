#define TA_PLUGIN_VERSION	0x0002

#ifndef EXPORT
#define EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct TAInfo {
	// Size of this data structure.
	unsigned int size;
	// Version of TA
	unsigned int version;
};

// Returns version of the API the plugin supports,
// which should just be TA_PLUGIN_VERSION.
// May pass in TA version info in a future version.
// Only function that must be exported.
EXPORT unsigned int __stdcall TAPluginGetVersion(const void *);

// Initialize plugin.  returns 0 on fail.  First paramater is
// info about TA, second parameter is for any return data future
// versions may need.
EXPORT int __stdcall TAPluginInitialize(const TAInfo *taInfo, void **);

// Can return null if does nothing to string.
EXPORT wchar_t * __stdcall TAPluginModifyStringPreSubstitution(const wchar_t *in);
EXPORT wchar_t * __stdcall TAPluginModifyStringPostSubstitution(const wchar_t *in);

EXPORT void __stdcall TAPluginActiveProfileList(int numActiveLists, const wchar_t **activeLists);

// Frees strings returned by previous function.
EXPORT void __stdcall TAPluginFree(void *in);


typedef unsigned int __stdcall TAPluginGetVersionType(void *);
typedef int __stdcall TAPluginInitializeType(const TAInfo *taInfo, void *);
typedef wchar_t * __stdcall TAPluginModifyStringPreSubstitutionType(const wchar_t *in);
typedef wchar_t * __stdcall TAPluginModifyStringPostSubstitutionType(const wchar_t *in);
// Note:  Values will stay valid until both pre and post substitution functions are called.
// Will by destroyed immediately afterwards, however.
typedef void __stdcall TAPluginActiveProfileListType(int numActiveLists, const wchar_t **activeLists);
typedef void __stdcall TAPluginFreeType(void *in);

#ifdef __cplusplus
}
#endif
