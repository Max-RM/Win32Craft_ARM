typedef unsigned int DWORD; typedef int BOOL; typedef void *HANDLE;
extern HANDLE GetProcessHeap(void);
extern BOOL HeapFree(HANDLE, DWORD, void *);
void *win32craft_LocalFree(void *memory)
{
    if (!memory) return (void *)0;
    return HeapFree(GetProcessHeap(), 0, memory) ? (void *)0 : memory;
}
