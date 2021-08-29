#include <windows.h>

#include "detours.h"

#include <vector>
#include <iostream>

std::vector<unsigned long long> g_handles;

#define UPDATE_ACCESS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

typedef struct _handle_information
{
	unsigned long process_id;
	unsigned long access;
	unsigned long long handle;
}handle_information, * phandle_information;

bool push_kernel_system(unsigned long long handle)
{
	HANDLE h = CreateFileA("\\\\.\\handle_access", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if (h == INVALID_HANDLE_VALUE) return false;

	handle_information info{ 0 };
	info.process_id = GetCurrentProcessId();
	info.access = 0x1fffff;
	info.handle = handle;

	DWORD r = 0;
	BOOL ret = DeviceIoControl(h, UPDATE_ACCESS, &info, sizeof(info), 0, 0, &r, 0);

	CloseHandle(h);
	return ret == TRUE;
}

/*
����һ���߳���������Ȩ
�еķ�����ϵͳ�᲻��ʱɨ����Ȩ��,�����ж�дȨ�޾ͻ�ĨȥȨ��,Ĩȥ�Ļ������мӻ���,��������ս��
�ҾͲ��ŷ�����ϵͳ�ļ���̱߳��һ�Ƶ��
*/
unsigned long __stdcall grant_access_thread(void*)
{
	while (true)
	{
		for (const auto& it : g_handles) push_kernel_system(it);
		Sleep(50);
	}
}

typedef HANDLE(WINAPI* FOpenProcess)(DWORD, BOOL, DWORD);
FOpenProcess _OpenProcess = OpenProcess;

HANDLE WINAPI MyOpenProcess(
	_In_ DWORD dwDesiredAccess,
	_In_ BOOL bInheritHandle,
	_In_ DWORD dwProcessId)
{
	// ����͵�Ȩ�޴򿪽���(��Ȼ������ϵͳ������)
	// �õ���Ȩ�޾�����ٽ�����Ȩ����
	HANDLE h = _OpenProcess(PROCESS_QUERY_INFORMATION, bInheritHandle, dwProcessId);
	if (h)
	{
		unsigned long long handle = (unsigned long long)h;
		push_kernel_system(handle);
		g_handles.push_back(handle);
	}
	return h;
}

typedef BOOL(WINAPI* FCloseHandle)(HANDLE);
FCloseHandle _CloseHandle = CloseHandle;

BOOL WINAPI MyCloseHandle(
	_In_ _Post_ptr_invalid_ HANDLE hObject)
{
	for (auto it = g_handles.begin(); it != g_handles.end(); it++)
	{
		if (*it == (unsigned long long)hObject)
		{
			g_handles.erase(it);
			break;
		}
	}

	return _CloseHandle(hObject);
}

bool init()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)_OpenProcess, MyOpenProcess);
	DetourAttach(&(PVOID&)_CloseHandle, MyCloseHandle);
	DetourTransactionCommit();

	CreateThread(0, 0, grant_access_thread, 0, 0, 0);
	MessageBoxA(0, "��Ȩģ�����", "��ܰ��ʾ", MB_OK | MB_ICONINFORMATION);
	return true;
}

static bool g = init();