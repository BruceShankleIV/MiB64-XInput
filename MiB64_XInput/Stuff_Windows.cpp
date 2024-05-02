/* Copyright (c) 2012 KrossX <krossx@live.com>
 * License: http://www.opensource.org/licenses/mit-license.html  MIT License
 */

//#include "general.h"
#include "psemupro.h"
#include "Input.h"
#include "Stuff.h"

#ifdef _WIN32

HWND h_display = NULL;
HINSTANCE h_instance = NULL;
WNDPROC h_winproc = NULL;

void ShowDialog(const wchar_t* message, const wchar_t* title)
{
	MessageBox(NULL, message,title, MB_OK);
}

void MouseThread()
{
	while(true)
	{
		mouse_event( MOUSEEVENTF_MOVE, 0, 0, 0, NULL);
		Sleep(50000);
	}
}

void SetPriority()
{
	if(bPriority)
	{
		const HANDLE currProc = GetCurrentProcess(); 
		SetPriorityClass(currProc, ABOVE_NORMAL_PRIORITY_CLASS);
	}
}

void KeepAwake(u8 mode)
{
	static HANDLE hMouseThread = NULL;

	if(bKeepAwake)
	switch(mode)
	{
	case KEEPAWAKE_INIT:
	{
		if(hMouseThread == NULL)
			hMouseThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MouseThread, 0, 0, NULL);
	}
	break;

	case KEEPAWAKE_CLOSE:
	{
		if(hMouseThread)
		{
			TerminateThread(hMouseThread,0);
			hMouseThread = NULL;
		}
	}
	break;
	}
}

void GetDisplay(void* pDisplay)
{
	if(IsWindow((HWND)pDisplay))
		h_display = (HWND)pDisplay;
	else if(IsWindow(*(HWND*)pDisplay)) // And hopefully doesn't crash...
		h_display = *(HWND*)pDisplay;
	else
		h_display = NULL;
}

LRESULT CALLBACK KeyboardProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	keyEvent newEvent;

	switch(msg)
	{
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		newEvent.evt = 1;
		newEvent.key = wParam;
		keyEventList.push_back(newEvent);
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		newEvent.evt = 2;
		newEvent.key = wParam;
		keyEventList.push_back(newEvent);
		break;

	case WM_DESTROY:
	case WM_QUIT:
		newEvent.evt = 1;
		newEvent.key = VK_ESCAPE;
		keyEventList.push_back(newEvent);
		break;
	}

	return CallWindowProcW(h_winproc, hWnd, msg, wParam, lParam);
}

u8 SwapPorts()
{
	static u8 swapIt = 0;
	static bool pressed = false;
	const bool currPress = !!(GetAsyncKeyState(0x30) >> 1);

	if(!pressed && currPress)
	{
		swapIt ^= 1;
		pressed = true;
	}
	else if(pressed && !currPress)
	{
		pressed = false;
	}

	return swapIt;
}

void KeyboardOpen()
{
	if(isPs2Emulator)
		h_winproc = (WNDPROC)SetWindowLongPtr(h_display, GWLP_WNDPROC, (LPARAM)KeyboardProc);
}

void KeyboardClose()
{
	if(isPs2Emulator)
		SetWindowLongPtr(h_display, GWLP_WNDPROC, (LPARAM)h_winproc);
}

void KeyboardCheck() {}

void ScrollLockStuff(bool init)
{
	static bool scrollLock = false;
	static bool scrollLockSaved = false;

	if (!scrollLockSaved && init)
	{
		scrollLock = GetKeyState(VK_SCROLL) & 0x1;
		scrollLockSaved = true;
	}
	else if (scrollLockSaved && !init)
	{
		if (scrollLock != (GetKeyState(VK_SCROLL) & 0x1))
		{
			keybd_event(VK_SCROLL, 0x45, KEYEVENTF_EXTENDEDKEY, 0);
			keybd_event(VK_SCROLL, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
		}
	}
}

struct scroll_auto
{
	scroll_auto() { ScrollLockStuff(true); }
	~scroll_auto() { ScrollLockStuff(false); }
} scrolly;

BOOL APIENTRY DllMain(HMODULE hInst, DWORD dwReason, LPVOID lpReserved)
{
	h_instance = hInst;

	switch(dwReason)
	{
	case DLL_PROCESS_ATTACH:
		break;

	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}

#endif //WIN32
