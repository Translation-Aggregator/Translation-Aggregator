#include <Shared/Shrink.h>
#include "MyDirectDraw.h"

class myIDirectDrawSurface : public IDirectDrawSurface, IDirectDrawSurface2, IDirectDrawSurface3 {
private:
	IDirectDrawSurface *m_pIDirectDrawSurface;
	IDirectDrawSurface2 *m_pIDirectDrawSurface2;
	IDirectDrawSurface3 *m_pIDirectDrawSurface3;
	ULONG refs;
	BOOL doubleBuffered;
public:

	myIDirectDrawSurface(LPDDSURFACEDESC desc, IDirectDrawSurface *pOriginal) {
		doubleBuffered = 0;
		if (desc->ddsCaps.dwCaps & DDSCAPS_FLIP)
			doubleBuffered = 1;
		refs = 1;
		m_pIDirectDrawSurface = pOriginal;
		if (NOERROR != pOriginal->QueryInterface(IID_IDirectDrawSurface2, (void**)&m_pIDirectDrawSurface2))
			m_pIDirectDrawSurface2 = 0;
		if (NOERROR != pOriginal->QueryInterface(IID_IDirectDrawSurface3, (void**)&m_pIDirectDrawSurface3))
			m_pIDirectDrawSurface3 = 0;
	}
	virtual ~myIDirectDrawSurface(void) {
	}

	HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObj) {
		*ppvObj = NULL;

		HRESULT hRes = NOERROR;
		if (IsEqualGUID(riid, IID_IUnknown)) {
			*ppvObj = (IUnknown*)(IDirectDrawSurface*)this;
		}
		else if (IsEqualGUID(riid, IID_IDirectDrawSurface)) {
			*ppvObj = (IDirectDrawSurface*)this;
		}
		else if (IsEqualGUID(riid, IID_IDirectDrawSurface2)) {
			*ppvObj = (IDirectDrawSurface2*)this;
		}
		else if (IsEqualGUID(riid, IID_IDirectDrawSurface3)) {
			*ppvObj = (IDirectDrawSurface3*)this;
		}
		else {
			return m_pIDirectDrawSurface->QueryInterface(riid, ppvObj); 
		}
		refs++;
		return hRes;
	}

	ULONG    __stdcall AddRef(void) {
		return ++refs;
	}

	ULONG    __stdcall Release(void) {
		--refs;
		if (!refs) {
			m_pIDirectDrawSurface->Release();
			if (m_pIDirectDrawSurface2) m_pIDirectDrawSurface2->Release();
			if (m_pIDirectDrawSurface3) m_pIDirectDrawSurface3->Release();
			delete this;
			return 0;
		}
		return refs;
	}
    /*** IDirectDrawSurface methods ***/
	HRESULT __stdcall AddAttachedSurface(LPDIRECTDRAWSURFACE a1) {
		return m_pIDirectDrawSurface->AddAttachedSurface(a1);
	}
    HRESULT __stdcall AddOverlayDirtyRect(LPRECT r) {
		return m_pIDirectDrawSurface->AddOverlayDirtyRect(r);
	}
    HRESULT __stdcall Blt(LPRECT a1,LPDIRECTDRAWSURFACE a2, LPRECT a3, DWORD a4, LPDDBLTFX a5) {
		HRESULT res = m_pIDirectDrawSurface->Blt(a1, a2, a3, a4, a5);
		/*if (a3->right - a3->left >300) {
			a1 = a1;
		}//*/
		return m_pIDirectDrawSurface->Blt(a1, a2, a3, a4, a5);
	}
    HRESULT __stdcall BltBatch(LPDDBLTBATCH a1, DWORD a2, DWORD a3) {
		return m_pIDirectDrawSurface->BltBatch(a1, a2, a3);
	}
    HRESULT __stdcall BltFast(DWORD a1, DWORD a2, LPDIRECTDRAWSURFACE a3, LPRECT a4, DWORD a5) {
		return m_pIDirectDrawSurface->BltFast(a1, a2, a3, a4, a5);
	}
    HRESULT __stdcall DeleteAttachedSurface(DWORD a1, LPDIRECTDRAWSURFACE a2) {
		return m_pIDirectDrawSurface->DeleteAttachedSurface(a1, a2);
	}
    HRESULT __stdcall EnumAttachedSurfaces(LPVOID a1, LPDDENUMSURFACESCALLBACK a2) {
		return m_pIDirectDrawSurface->EnumAttachedSurfaces(a1, a2);
	}
    HRESULT __stdcall EnumOverlayZOrders(DWORD a1, LPVOID a2, LPDDENUMSURFACESCALLBACK a3) {
		return m_pIDirectDrawSurface->EnumOverlayZOrders(a1, a2, a3);
	}
    HRESULT __stdcall Flip(LPDIRECTDRAWSURFACE a1, DWORD a2) {
		return m_pIDirectDrawSurface->Flip(a1, a2);
	}
    HRESULT __stdcall GetAttachedSurface(LPDDSCAPS a1, LPDIRECTDRAWSURFACE FAR *a2) {
		return m_pIDirectDrawSurface->GetAttachedSurface(a1, a2);
	}
    HRESULT __stdcall GetBltStatus(DWORD a1) {
		return m_pIDirectDrawSurface->GetBltStatus(a1);
	}
    HRESULT __stdcall GetCaps(LPDDSCAPS a1) {
		return m_pIDirectDrawSurface->GetCaps(a1);
	}
    HRESULT __stdcall GetClipper(LPDIRECTDRAWCLIPPER FAR* a1) {
		return m_pIDirectDrawSurface->GetClipper(a1);
	}
    HRESULT __stdcall GetColorKey(DWORD a1, LPDDCOLORKEY a2) {
		return m_pIDirectDrawSurface->GetColorKey(a1, a2);
	}
    HRESULT __stdcall GetDC(HDC FAR * a1) {
		return m_pIDirectDrawSurface->GetDC(a1);
	}
    HRESULT __stdcall GetFlipStatus(DWORD a1) {
		return m_pIDirectDrawSurface->GetFlipStatus(a1);
	}
    HRESULT __stdcall GetOverlayPosition(LPLONG a1, LPLONG a2) {
		return m_pIDirectDrawSurface->GetOverlayPosition(a1, a2);
	}
    HRESULT __stdcall GetPalette(LPDIRECTDRAWPALETTE FAR* a1) {
		return m_pIDirectDrawSurface->GetPalette(a1);
	}
    HRESULT __stdcall GetPixelFormat(LPDDPIXELFORMAT a1) {
		return m_pIDirectDrawSurface->GetPixelFormat(a1);
	}
    HRESULT __stdcall GetSurfaceDesc(LPDDSURFACEDESC a1) {
		return m_pIDirectDrawSurface->GetSurfaceDesc(a1);
	}
    HRESULT __stdcall Initialize(LPDIRECTDRAW a1, LPDDSURFACEDESC a2) {
		return m_pIDirectDrawSurface->Initialize(a1, a2);
	}
    HRESULT __stdcall IsLost() {
		return m_pIDirectDrawSurface->IsLost();
	}
    HRESULT __stdcall Lock(LPRECT a1,LPDDSURFACEDESC a2,DWORD a3,HANDLE a4) {
		return m_pIDirectDrawSurface->Lock(a1, a2, a3, a4);
	}
    HRESULT __stdcall ReleaseDC(HDC hDC) {
		return m_pIDirectDrawSurface->ReleaseDC(hDC);
	}
    HRESULT __stdcall Restore() {
		return m_pIDirectDrawSurface->Restore();
	}
    HRESULT __stdcall SetClipper(LPDIRECTDRAWCLIPPER a1) {
		return m_pIDirectDrawSurface->SetClipper(a1);
	}
    HRESULT __stdcall SetColorKey(DWORD a1, LPDDCOLORKEY a2) {
		return m_pIDirectDrawSurface->SetColorKey(a1, a2);
	}
    HRESULT __stdcall SetOverlayPosition(LONG a1, LONG a2) {
		return m_pIDirectDrawSurface->SetOverlayPosition(a1, a2);
	}
    HRESULT __stdcall SetPalette(LPDIRECTDRAWPALETTE a1) {
		return m_pIDirectDrawSurface->SetPalette(a1);
	}
    HRESULT __stdcall Unlock(LPVOID a1) {
		return m_pIDirectDrawSurface->Unlock(a1);
	}
    HRESULT __stdcall UpdateOverlay(LPRECT a1, LPDIRECTDRAWSURFACE a2, LPRECT a3, DWORD a4, LPDDOVERLAYFX a5) {
		return m_pIDirectDrawSurface->UpdateOverlay(a1, a2, a3, a4, a5);
	}
    HRESULT __stdcall UpdateOverlayDisplay(DWORD a1) {
		return m_pIDirectDrawSurface->UpdateOverlayDisplay(a1);
	}
    HRESULT __stdcall UpdateOverlayZOrder(DWORD a1, LPDIRECTDRAWSURFACE a2) {
		return m_pIDirectDrawSurface->UpdateOverlayZOrder(a1, a2);
	}


	HRESULT __stdcall AddAttachedSurface(LPDIRECTDRAWSURFACE2 a1) {
		return m_pIDirectDrawSurface2->AddAttachedSurface(a1);
	}

	//void PreBlt(LPRECT srcRect,LPDIRECTDRAWSURFACE3 surface) {
		//if (
	//}

	HRESULT __stdcall Blt(LPRECT dstRect,LPDIRECTDRAWSURFACE2 surface, LPRECT srcRect, DWORD dwFlags, LPDDBLTFX a5) {
		return m_pIDirectDrawSurface2->Blt(dstRect, surface, srcRect, dwFlags, a5);
	}
    HRESULT __stdcall BltFast(DWORD a1, DWORD a2, LPDIRECTDRAWSURFACE2 a3, LPRECT a4, DWORD a5) {
		return m_pIDirectDrawSurface2->BltFast(a1, a2, a3, a4, a5);
	}
    HRESULT __stdcall DeleteAttachedSurface(DWORD a1, LPDIRECTDRAWSURFACE2 a2) {
		return m_pIDirectDrawSurface2->DeleteAttachedSurface(a1, a2);
	}
    HRESULT __stdcall Flip(LPDIRECTDRAWSURFACE2 a1, DWORD a2) {
		return m_pIDirectDrawSurface2->Flip(a1, a2);
	}
    HRESULT __stdcall GetAttachedSurface(LPDDSCAPS a1, LPDIRECTDRAWSURFACE2 FAR *a2) {
		return m_pIDirectDrawSurface2->GetAttachedSurface(a1, a2);
	}
    HRESULT __stdcall UpdateOverlay(LPRECT a1, LPDIRECTDRAWSURFACE2 a2, LPRECT a3, DWORD a4, LPDDOVERLAYFX a5) {
		return m_pIDirectDrawSurface2->UpdateOverlay(a1, a2, a3, a4, a5);
	}
    HRESULT __stdcall UpdateOverlayZOrder(DWORD a1, LPDIRECTDRAWSURFACE2 a2) {
		return m_pIDirectDrawSurface2->UpdateOverlayZOrder(a1, a2);
	}

	HRESULT __stdcall AddAttachedSurface(LPDIRECTDRAWSURFACE3 a1) {
		return m_pIDirectDrawSurface3->AddAttachedSurface(a1);
	}

	HRESULT __stdcall Blt(LPRECT dstRect,LPDIRECTDRAWSURFACE3 surface, LPRECT srcRect, DWORD dwFlags, LPDDBLTFX a5) {
		/*if (srcRect->right - srcRect->left > 100) {
			srcRect=srcRect;
			DDSURFACEDESC desc;
			desc.dwSize = sizeof(desc);
			desc.dwFlags = DDSD_HEIGHT | DDSD_WIDTH;
			if (S_OK == surface->GetSurfaceDesc(&desc)) {
				srcRect=srcRect;
			}
		}
		if (!doubleBuffered && !dstRect->top && !dstRect->left &&
			 dstRect->right-dstRect->left == srcRect->right-srcRect->left &&
			 dstRect->bottom-dstRect->top == srcRect->bottom-srcRect->top) {
			DDSURFACEDESC desc;
			desc.dwSize = sizeof(desc);
			desc.dwFlags = DDSD_HEIGHT | DDSD_WIDTH;
			if (S_OK == surface->GetSurfaceDesc(&desc) &&
				 dstRect->right-dstRect->left == desc.dwWidth &&
				 dstRect->bottom-dstRect->top == desc.dwHeight) {
				PreBlt(srcRect, surface);
			}
		}//*/
		return m_pIDirectDrawSurface3->Blt(dstRect, surface, srcRect, dwFlags, a5);
	}
    HRESULT __stdcall BltFast(DWORD a1, DWORD a2, LPDIRECTDRAWSURFACE3 a3, LPRECT a4, DWORD a5) {
		return m_pIDirectDrawSurface3->BltFast(a1, a2, a3, a4, a5);
	}
    HRESULT __stdcall DeleteAttachedSurface(DWORD a1, LPDIRECTDRAWSURFACE3 a2) {
		return m_pIDirectDrawSurface3->DeleteAttachedSurface(a1, a2);
	}
    HRESULT __stdcall Flip(LPDIRECTDRAWSURFACE3 a1, DWORD a2) {
		return m_pIDirectDrawSurface3->Flip(a1, a2);
	}
    HRESULT __stdcall GetAttachedSurface(LPDDSCAPS a1, LPDIRECTDRAWSURFACE3 FAR *a2) {
		return m_pIDirectDrawSurface3->GetAttachedSurface(a1, a2);
	}
    HRESULT __stdcall UpdateOverlay(LPRECT a1, LPDIRECTDRAWSURFACE3 a2, LPRECT a3, DWORD a4, LPDDOVERLAYFX a5) {
		return m_pIDirectDrawSurface3->UpdateOverlay(a1, a2, a3, a4, a5);
	}
    HRESULT __stdcall UpdateOverlayZOrder(DWORD a1, LPDIRECTDRAWSURFACE3 a2) {
		return m_pIDirectDrawSurface3->UpdateOverlayZOrder(a1, a2);
	}


    /*** Added in the v2 interface ***/
	HRESULT __stdcall GetDDInterface(LPVOID FAR *a1) {
		if (m_pIDirectDrawSurface3) return m_pIDirectDrawSurface3->GetDDInterface(a1);
		return m_pIDirectDrawSurface2->GetDDInterface(a1);
	}
	HRESULT __stdcall PageLock(DWORD a1) {
		if (m_pIDirectDrawSurface3) return m_pIDirectDrawSurface3->PageLock(a1);
		return m_pIDirectDrawSurface2->PageLock(a1);
	}
	HRESULT __stdcall PageUnlock(DWORD a1) {
		if (m_pIDirectDrawSurface3) m_pIDirectDrawSurface3->PageUnlock(a1);
		return m_pIDirectDrawSurface2->PageUnlock(a1);
	}

	HRESULT __stdcall SetSurfaceDesc(LPDDSURFACEDESC a1, DWORD a2) {
		return m_pIDirectDrawSurface3->SetSurfaceDesc(a1, a2);
	}
};

class myIDirectDraw : public IDirectDraw, public IDirectDraw2 //, public IDirectDraw7
{
private:
	IDirectDraw2 *m_pIDirectDraw2;
   	IDirectDraw *m_pIDirectDraw;
	ULONG refs;
public:
	myIDirectDraw(IDirectDraw *pOriginal) {
		//while(1);
		refs = 1;
		m_pIDirectDraw = pOriginal;
		m_pIDirectDraw2 = 0;
		//m_pIDirectDraw7 = pOriginal2;
	}
	virtual ~myIDirectDraw(void) {
	}

	HRESULT  __stdcall QueryInterface(REFIID riid, void** ppvObj) {
		//+		riid	{B3A6F3E0-2B43-11CF-A2DE-00AA00B93356}	const _GUID &

		*ppvObj = NULL;

		HRESULT hRes = NOERROR;
		if (IsEqualGUID(riid, IID_IUnknown)) {
			*ppvObj = (IUnknown*)(IDirectDraw*)this;
			++refs;
		}
		else if (IsEqualGUID(riid, IID_IDirectDraw)) {
			*ppvObj = (IDirectDraw*)this;
			++refs;
		}
		else if (IsEqualGUID(riid, IID_IDirectDraw2)) {
			if (!m_pIDirectDraw2) {
				hRes = m_pIDirectDraw->QueryInterface(riid, ppvObj); 
				if (hRes == NOERROR)
					m_pIDirectDraw2 = (IDirectDraw2*)*ppvObj;
			}
			if (hRes == NOERROR) {
				*ppvObj = (IDirectDraw2*)this;
				++refs;
			}
		}
		else {
			hRes = m_pIDirectDraw->QueryInterface(riid, ppvObj); 
		}
		return hRes;
	}

	ULONG    __stdcall AddRef(void) {
		return ++refs;
	}

	ULONG    __stdcall Release(void) {
		--refs;
		if (!refs) {
			if (m_pIDirectDraw2) {
				m_pIDirectDraw2->Release();
			}
			m_pIDirectDraw->Release();
			delete this;
			return 0;
		}
		return refs;
	}

	HRESULT __stdcall Compact() {
		return m_pIDirectDraw->Compact();
	}
	HRESULT __stdcall CreateClipper(DWORD a1, LPDIRECTDRAWCLIPPER FAR* a2, IUnknown FAR * a3) {
		return m_pIDirectDraw->CreateClipper(a1, a2, a3);
	}
	HRESULT __stdcall CreatePalette(DWORD a1, LPPALETTEENTRY a2, LPDIRECTDRAWPALETTE FAR* a3, IUnknown FAR * a4) {
		return m_pIDirectDraw->CreatePalette(a1, a2, a3, a4);
	}
	/*
	HRESULT __stdcall CreateSurface(LPDDSURFACEDESC2 a1, LPDIRECTDRAWSURFACE7 FAR * a2, IUnknown FAR *a3) {
		return m_pIDirectDraw7->CreateSurface(a1, a2, a3);
	}
	HRESULT __stdcall DuplicateSurface(LPDIRECTDRAWSURFACE7 a1, LPDIRECTDRAWSURFACE7 FAR * a2) {
		return m_pIDirectDraw7->DuplicateSurface(a1, a2);
	}
	HRESULT __stdcall EnumDisplayModes(DWORD a1, LPDDSURFACEDESC2 a2, LPVOID a3, LPDDENUMMODESCALLBACK2 a4) {
		return m_pIDirectDraw7->EnumDisplayModes(a1, a2, a3, a4);
	}
	HRESULT __stdcall EnumSurfaces(DWORD a1, LPDDSURFACEDESC2 a2, LPVOID a3, LPDDENUMSURFACESCALLBACK7 a4) {
		return m_pIDirectDraw->EnumSurfaces(a1, a2, a3, a4);
	}
	//*/

	HRESULT __stdcall FlipToGDISurface() {
		return m_pIDirectDraw->FlipToGDISurface();
	}
	HRESULT __stdcall GetCaps(LPDDCAPS a1, LPDDCAPS a2) {
		return m_pIDirectDraw->GetCaps(a1, a2);
	}

	/*
	HRESULT __stdcall GetDisplayMode(LPDDSURFACEDESC2 a) {
		return m_pIDirectDraw7->GetDisplayMode(a);
	}
	HRESULT __stdcall GetGDISurface(LPDIRECTDRAWSURFACE7 FAR *a1) {
		return m_pIDirectDraw7->GetGDISurface(a1);
	}
	//*/
	HRESULT __stdcall GetFourCCCodes(LPDWORD a1, LPDWORD a2) {
		return m_pIDirectDraw->GetFourCCCodes(a1, a2);
	}
	HRESULT __stdcall GetMonitorFrequency(LPDWORD a) {
		return m_pIDirectDraw->GetMonitorFrequency(a);
	}
	HRESULT __stdcall GetScanLine(LPDWORD a) {
		return m_pIDirectDraw->GetScanLine(a);
	}
	HRESULT __stdcall GetVerticalBlankStatus(LPBOOL a) {
		return m_pIDirectDraw->GetVerticalBlankStatus(a);
	}
	HRESULT __stdcall Initialize(GUID FAR *a) {
		return m_pIDirectDraw->Initialize(a);
	}
	HRESULT __stdcall RestoreDisplayMode() {
		return m_pIDirectDraw->RestoreDisplayMode();
	}
	HRESULT __stdcall SetCooperativeLevel(HWND hWnd, DWORD a2) {
		return m_pIDirectDraw->SetCooperativeLevel(hWnd, a2);
	}
    HRESULT __stdcall SetDisplayMode(DWORD a1, DWORD a2, DWORD a3, DWORD a4, DWORD a5) {
		return m_pIDirectDraw2->SetDisplayMode(a1, a2, a3, a4, a5);
	}
    HRESULT __stdcall WaitForVerticalBlank(DWORD a1, HANDLE a2) {
		return m_pIDirectDraw->WaitForVerticalBlank(a1, a2);
	}
    /*** Added in the v2 interface ***/
    HRESULT __stdcall GetAvailableVidMem(LPDDSCAPS a1, LPDWORD a2, LPDWORD a3) {
		return m_pIDirectDraw2->GetAvailableVidMem(a1, a2, a3);
	}
    /*** Added in the V4 Interface ***/
	/*
    HRESULT __stdcall GetSurfaceFromDC(HDC a1, LPDIRECTDRAWSURFACE7 *a2) {
		return m_pIDirectDraw7->GetSurfaceFromDC(a1, a2);
	}
	HRESULT __stdcall RestoreAllSurfaces() {
		return m_pIDirectDraw7->RestoreAllSurfaces();
	}
	HRESULT __stdcall TestCooperativeLevel() {
		return m_pIDirectDraw7->TestCooperativeLevel();
	}
	HRESULT __stdcall GetDeviceIdentifier(LPDDDEVICEIDENTIFIER2 a1, DWORD a2) {
		return m_pIDirectDraw7->GetDeviceIdentifier(a1, a2);
	}
    HRESULT __stdcall StartModeTest(LPSIZE a1, DWORD a2, DWORD a3) {
		return m_pIDirectDraw7->StartModeTest(a1, a2, a3);
	}
	HRESULT __stdcall EvaluateMode(DWORD a1, DWORD * a2) {
		return m_pIDirectDraw7->EvaluateMode(a1, a2);
	}
	//*/

	HRESULT __stdcall CreateSurface(LPDDSURFACEDESC desc, LPDIRECTDRAWSURFACE FAR *a2, IUnknown FAR *a3) {
		HRESULT res = m_pIDirectDraw->CreateSurface(desc, a2, a3);
		if (res == NOERROR && (desc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)) {
			*a2 = (IDirectDrawSurface*) new myIDirectDrawSurface(desc, *a2);
		}
		return res;
	}
	HRESULT __stdcall SetDisplayMode(DWORD a1, DWORD a2,DWORD a3) {
		return m_pIDirectDraw->SetDisplayMode(a1, a2, a3);
	}
	HRESULT __stdcall DuplicateSurface(LPDIRECTDRAWSURFACE a1, LPDIRECTDRAWSURFACE FAR * a2) {
		return m_pIDirectDraw->DuplicateSurface(a1, a2);
	}
	HRESULT __stdcall EnumDisplayModes(DWORD a1, LPDDSURFACEDESC a2, LPVOID a3, LPDDENUMMODESCALLBACK a4) {
		return m_pIDirectDraw->EnumDisplayModes(a1, a2, a3, a4);
	}
	HRESULT __stdcall EnumSurfaces(DWORD a1, LPDDSURFACEDESC a2, LPVOID a3, LPDDENUMSURFACESCALLBACK a4) {
		return m_pIDirectDraw->EnumSurfaces(a1, a2, a3, a4);
	}
	HRESULT __stdcall GetDisplayMode(LPDDSURFACEDESC a) {
		return m_pIDirectDraw->GetDisplayMode(a);
	}
	HRESULT __stdcall GetGDISurface(LPDIRECTDRAWSURFACE FAR *a1) {
		return m_pIDirectDraw->GetGDISurface(a1);
	}
};

HRESULT WINAPI MyDirectDrawCreate( GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter) {
	HRESULT res = DirectDrawCreate(lpGUID, lplpDD, pUnkOuter);
	while(1) {
		if (1) break;
	}
	if (res == S_OK && !lpGUID) {
	   	//IDirectDraw7 *pIDirectDraw7;
		//int res2 = lplpDD[0]->QueryInterface(CLSID_DirectDraw7, (void**)&pIDirectDraw7);
		//if (res2 == NOERROR) {
		//}
		*lplpDD = new myIDirectDraw(*lplpDD);
	}
	return res;
}

HRESULT WINAPI MyDirectDrawCreateEx(GUID FAR * lpGuid, LPVOID  *lplpDD, REFIID  iid,IUnknown FAR *pUnkOuter) {
	return DirectDrawCreateEx(lpGuid, lplpDD, iid, pUnkOuter);
}
