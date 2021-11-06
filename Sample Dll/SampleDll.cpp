// Not sure how compiler independent this is.  Change as needed.
#define EXPORT __declspec(dllexport)

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include "TAPlugin.h"

EXPORT unsigned int TAPluginGetVersion(const void *)
{
	return TA_PLUGIN_VERSION;
}

EXPORT int TAPluginInitialize(const TAInfo *taInfo, void **)
{
	// At the moment, don't need to check taInfo version.
	return 1;
}

// Can return null if does nothing to string.
EXPORT wchar_t * TAPluginModifyStringPreSubstitution(wchar_t *in)
{
	if (!wcsstr(in, L"cheese")) return 0;

	wchar_t *out = (wchar_t*) malloc(2*wcslen(in) * sizeof(wchar_t));

	wchar_t *outPos = out;
	while (*in)
	{
		if (!wcsncmp(in, L"cheese", 5))
		{
			wcscpy(outPos, L"carrot cake");
			outPos += wcslen(outPos);
			in += wcslen(L"cheese");
		}
		else
			in++[0] = outPos++[0];
	}
	*outPos = 0;
	return out;
}

// Frees strings returned by previous function.
EXPORT void TAPluginFree(void *in)
{
	free(in);
}
