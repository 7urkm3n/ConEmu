
/*
Copyright (c) 2014 Maximus5
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

enum ProcessDpiAwareness
{ 
	Process_DPI_Unaware            = 0,
	Process_System_DPI_Aware       = 1,
	Process_Per_Monitor_DPI_Aware  = 2
};

enum MonitorDpiType
{ 
  MDT_Effective_DPI  = 0,
  MDT_Angular_DPI    = 1,
  MDT_Raw_DPI        = 2,
  MDT_Default        = MDT_Effective_DPI
};


class CDpiAware
{
public:
	static HRESULT setProcessDPIAwareness()
	{
		HRESULT hr = E_FAIL;
		HMODULE hUser;
		// Actual export name - "SetProcessDpiAwarenessInternal"
		typedef HRESULT (WINAPI* SetProcessDPIAwareness_t)(ProcessDpiAwareness value);
		SetProcessDPIAwareness_t fWin81;
		// "SetProcessDPIAware
		typedef BOOL (WINAPI* SetProcessDPIAware_t)(void);
		SetProcessDPIAware_t fVista;


		if ((hUser = GetModuleHandle(L"user32.dll")) == NULL)
		{
			goto wrap;
		}

		if ((fWin81 = (SetProcessDPIAwareness_t)GetProcAddress(hUser, "SetProcessDpiAwarenessInternal")) != NULL)
		{
			hr = fWin81(Process_Per_Monitor_DPI_Aware);
			goto wrap;
		}

		if ((fVista = (SetProcessDPIAware_t)GetProcAddress(hUser, "SetProcessDPIAware")) != NULL)
		{
			hr = fVista() ? S_OK : E_ACCESSDENIED;
			goto wrap;
		}

	wrap:
		return hr;
	}
};

class CDpiForDialog
{
protected:
	HWND mh_Dlg;

	int mn_InitFontHeight;
	LOGFONT mlf_InitFont;
	int mn_CurFontHeight;
	LOGFONT mlf_CurFont;

	HFONT mh_OldFont, mh_CurFont;

public:
	CDpiForDialog()
	{
		mh_Dlg = NULL;
		mn_InitFontHeight = 8;
		ZeroStruct(mlf_InitFont);
		mn_CurFontHeight = 0;
		ZeroStruct(mlf_CurFont);
		mh_OldFont = mh_CurFont = NULL;
	};

	~CDpiForDialog()
	{
	};

	bool Attach(HWND hWnd, int nCurDpi)
	{
		mh_Dlg = hWnd;

		mh_OldFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
		if ((mh_OldFont != NULL)
			&& (GetObject(mh_OldFont, sizeof(mlf_InitFont), &mlf_InitFont) > 0))
		{
			mn_InitFontHeight = mlf_InitFont.lfHeight;
		}
		else
		{
			mn_InitFontHeight = 8;
			ZeroStruct(mlf_InitFont);
			mlf_InitFont.lfHeight = mn_InitFontHeight;
			lstrcpyn(mlf_InitFont.lfFaceName, L"MS Shell Dlg", countof(mlf_InitFont.lfFaceName));
			mlf_InitFont.lfWeight = 400;
			mlf_InitFont.lfCharSet = DEFAULT_CHARSET;
		}

		// Up to Windows 8 - OS will care of dialog scaling
		// And what will happens in Windows 8.1?
		// If `Per-monitor` dpi was choosed in the OS settings,
		// we need to re-scale our dialog manually!
		// But if one dpi was choosed for all monitors?

		/*
		if (nCurDpi != 96)
		{
			if (!SetDialogDPI(nCurDpi))
				return false;
		}
		*/

		return true;
	}

	bool SetDialogDPI(int dpi)
	{
		mn_CurFontHeight = (mn_InitFontHeight * dpi / 92);
		mlf_CurFont = mlf_InitFont;
		mlf_CurFont.lfHeight = mn_CurFontHeight;
		mlf_CurFont.lfWidth = 0; // Font mapper fault

		if (mn_CurFontHeight == 0 || mn_InitFontHeight == 0)
			return false;

		DWORD dwStyle = GetWindowLong(mh_Dlg, GWL_STYLE);
		DWORD dwStyleEx = GetWindowLong(mh_Dlg, GWL_EXSTYLE);

		RECT rcClient = {};
		if (!GetClientRect(mh_Dlg, &rcClient))
			return false;

		_ASSERTE(rcClient.left==0 && rcClient.top==0);
		RECT rcWnd = {0, 0, rcClient.right * mn_CurFontHeight / mn_InitFontHeight, rcClient.bottom * mn_CurFontHeight / mn_InitFontHeight};
		if (!AdjustWindowRectEx(&rcWnd, dwStyle, FALSE, dwStyleEx))
			return false;

		HFONT hf = CreateFontIndirect(&mlf_CurFont);
		if (hf == NULL)
			return false;

		SendMessage(mh_Dlg, WM_SETFONT, (WPARAM)hf, FALSE);
		SetWindowPos(mh_Dlg, NULL, 0, 0, rcWnd.right-rcWnd.left, rcWnd.bottom-rcWnd.top, SWP_NOMOVE|SWP_NOZORDER);

		HWND hc = NULL;
		while ((hc = FindWindowEx(mh_Dlg, hc, NULL, NULL)) != NULL)
		{
			RECT rcChild = {};
			if (GetWindowRect(hc, &rcChild) && MapWindowPoints(NULL, mh_Dlg, (LPPOINT)&rcChild, 2))
			{
				int newX = rcChild.left   * mn_CurFontHeight / mn_InitFontHeight;
				int newY = rcChild.top    * mn_CurFontHeight / mn_InitFontHeight;
				int newW = (rcChild.right - rcChild.left) * mn_CurFontHeight / mn_InitFontHeight;
				int newH = (rcChild.bottom - rcChild.top) * mn_CurFontHeight / mn_InitFontHeight;
				SetWindowPos(hc, NULL, newX, newY, newW, newH, SWP_NOZORDER);
				SendMessage(hc, WM_SETFONT, (WPARAM)hf, TRUE/*Redraw immediately*/);
			}
		}

		if (mh_CurFont != hf)
			DeleteObject(mh_CurFont);
		mh_CurFont = hf;

		return true;
	};

	void Detach()
	{
	};
};