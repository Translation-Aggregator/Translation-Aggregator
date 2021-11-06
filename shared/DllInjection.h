// Use different names for release and debug version.
#ifndef _DEBUG
#define DLL_NAME L"App Translator.dll"
#else
#define DLL_NAME L"App Translator debug.dll"
#endif

#define CREATE_THREAD_ACCESS (PROCESS_CREATE_THREAD | \
                              PROCESS_QUERY_INFORMATION | \
                              PROCESS_VM_OPERATION | \
                              PROCESS_VM_WRITE | \
                              PROCESS_VM_READ \
                             )

unsigned long GetTargetThreadIdFromProcname(char *procName);
unsigned long GetTargetProcessIdFromProcname(char *procName);
int InjectIntoProcess(int pid);
int getDebugPriv();
BOOL CALLBACK FindProcessWindow(HWND hWnd, LPARAM lParam);
int InjectDll(HANDLE hProcess, HINSTANCE hInst=0);

HANDLE Run(const wchar_t *exe, DWORD *processId, LCID lcid);
