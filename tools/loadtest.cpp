#include <windows.h>
#include <cstdio>
int main(int argc, char** argv) {
    HMODULE h = LoadLibraryA(argv[1]);
    if (!h) { printf("LOAD_FAIL %lu\n", GetLastError()); return 1; }
    printf("LOAD_OK base=%p\n", (void*)h);
    fflush(stdout);
    ExitProcess(0); // kill worker threads without unloading
}
