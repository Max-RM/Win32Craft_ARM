#include <stdio.h>
__declspec(dllimport) void *__stdcall LocalFree(void *);
__declspec(dllimport) void *__stdcall LocalAlloc(unsigned, unsigned long long);
int main(void){ void *p = LocalAlloc(0, 64); void *r = LocalFree(p); printf("LocalFree via shim returned %p (expect 0)\n", r); return r != 0; }
