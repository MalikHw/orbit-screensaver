#include <windows.h>
#include <shlobj.h>
#include <string.h>
#include <cstdio>
#pragma comment(lib, "shell32.lib")



// tiny stub - windows calls this .scr, we just forward to the real exe in appdata
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int) {
    // find real exe
    char regPath[MAX_PATH] = "";
    DWORD sz = MAX_PATH;
    HKEY key;
    if(RegOpenKeyExA(HKEY_CURRENT_USER,"Software\\Orbit",0,KEY_READ,&key)==ERROR_SUCCESS){
        RegQueryValueExA(key,"InstallDir",0,0,(LPBYTE)regPath,&sz);
        RegCloseKey(key);
    }
    if(!regPath[0]){
        SHGetFolderPathA(NULL,CSIDL_LOCAL_APPDATA,NULL,0,regPath);
        strcat(regPath,"\\orbit");
    }

    char exePath[MAX_PATH];
    snprintf(exePath,sizeof(exePath),"%s\\orbit_screensaver.exe",regPath);

    // forward original command line args
    char* cmdLine = GetCommandLineA();
    // skip exe name
    char* args = cmdLine;
    if(*args=='"'){args++;while(*args&&*args!='"')args++;if(*args)args++;}
    else{while(*args&&*args!=' ')args++;}
    while(*args==' ')args++;

    char fullCmd[MAX_PATH+256];
    snprintf(fullCmd,sizeof(fullCmd),"\"%s\" %s",exePath,args);

    STARTUPINFOA si={sizeof(si)};
    PROCESS_INFORMATION pi={};
    CreateProcessA(NULL, fullCmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if(pi.hProcess) {
        WaitForSingleObject(pi.hProcess, INFINITE); // keep stub alive
        CloseHandle(pi.hProcess);
    }
    if(pi.hThread) CloseHandle(pi.hThread);
}
