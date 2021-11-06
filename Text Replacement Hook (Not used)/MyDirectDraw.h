#pragma once
#include <ddraw.h>

HRESULT WINAPI MyDirectDrawCreate( GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter);
HRESULT WINAPI MyDirectDrawCreateEx(GUID FAR * lpGuid, LPVOID  *lplpDD, REFIID  iid,IUnknown FAR *pUnkOuter);

