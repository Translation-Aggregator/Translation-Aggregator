#include <Shared/Shrink.h>
#include "ProcessUtil.h"

void LaunchJap(wchar_t *app, wchar_t *args)
{
	SetEnvironmentVariableA("__COMPAT_LAYER", "#ApplicationLocale");
	SetEnvironmentVariableA("AppLocaleID", "0411");
	ShellExecuteW(0, 0, app, args, 0, 1);
}
