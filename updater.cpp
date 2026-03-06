// thx github for the api
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <string>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")

static std::string getExeDir() {
    char buf[MAX_PATH]; GetModuleFileNameA(NULL,buf,MAX_PATH);
    std::string s(buf); return s.substr(0,s.rfind('\\'));
}

static bool downloadFile(const char* url, const char* destPath) {
    // parse url: skip https://
    const char* host_start = url;
    if(strncmp(url,"https://",8)==0) host_start+=8;
    const char* path_start = strchr(host_start,'/');
    if(!path_start) return false;

    std::string host(host_start, path_start-host_start);
    std::string path(path_start);

    wchar_t whost[256], wpath[512];
    MultiByteToWideChar(CP_ACP,0,host.c_str(),-1,whost,256);
    MultiByteToWideChar(CP_ACP,0,path.c_str(),-1,wpath,512);

    HINTERNET hSession=WinHttpOpen(L"OrbitUpdater/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!hSession) return false;
    HINTERNET hConnect=WinHttpConnect(hSession,whost,INTERNET_DEFAULT_HTTPS_PORT,0);
    if(!hConnect){WinHttpCloseHandle(hSession);return false;}
    HINTERNET hRequest=WinHttpOpenRequest(hConnect,L"GET",wpath,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if(!hRequest){WinHttpCloseHandle(hConnect);WinHttpCloseHandle(hSession);return false;}
    WinHttpAddRequestHeaders(hRequest,L"User-Agent: OrbitUpdater",-1,WINHTTP_ADDREQ_FLAG_ADD);

    bool ok=false;
    if(WinHttpSendRequest(hRequest,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)
       &&WinHttpReceiveResponse(hRequest,NULL)){
        FILE* f=fopen(destPath,"wb");
        if(f){
            char buf[8192]; DWORD read=0;
            while(WinHttpReadData(hRequest,buf,sizeof(buf),&read)&&read>0)
                fwrite(buf,1,read,f);
            fclose(f); ok=true;
        }
    }
    WinHttpCloseHandle(hRequest);WinHttpCloseHandle(hConnect);WinHttpCloseHandle(hSession);
    return ok;
}

static bool extractZip(const char* zipPath, const char* destDir) {
    // Use PowerShell to extract - no extra deps needed
    char cmd[1024];
    snprintf(cmd,sizeof(cmd),
        "powershell -NoProfile -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\"",
        zipPath, destDir);
    int ret = system(cmd);
    return ret==0;
}

static std::string fetchLatestTag() {
    std::string result="";
    HINTERNET hSession=WinHttpOpen(L"OrbitUpdater/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!hSession) return result;
    HINTERNET hConnect=WinHttpConnect(hSession,L"api.github.com",INTERNET_DEFAULT_HTTPS_PORT,0);
    if(!hConnect){WinHttpCloseHandle(hSession);return result;}
    HINTERNET hRequest=WinHttpOpenRequest(hConnect,L"GET",
        L"/repos/MalikHw/orbit-screensaver/releases/latest",
        NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if(!hRequest){WinHttpCloseHandle(hConnect);WinHttpCloseHandle(hSession);return result;}
    WinHttpAddRequestHeaders(hRequest,L"User-Agent: OrbitUpdater",-1,WINHTTP_ADDREQ_FLAG_ADD);
    if(WinHttpSendRequest(hRequest,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)
       &&WinHttpReceiveResponse(hRequest,NULL)){
        char buf[4096]="";DWORD read=0;
        WinHttpReadData(hRequest,buf,sizeof(buf)-1,&read);buf[read]=0;
        const char* p=strstr(buf,"\"tag_name\":");
        if(p){p+=11;while(*p=='"'||*p==' ')p++;char tag[64]="";int i=0;while(*p&&*p!='"'&&i<63)tag[i++]=*p++;tag[i]=0;result=tag;}
    }
    WinHttpCloseHandle(hRequest);WinHttpCloseHandle(hConnect);WinHttpCloseHandle(hSession);
    return result;
}

int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int){
    std::string exeDir=getExeDir();
    std::string scrPath=exeDir+"\\orbit_screensaver.scr";

    // show simple progress window
    MessageBoxA(NULL,
        "Orbit Updater\n\nWaiting for screensaver to close, then updating...\n\nThis window will close automatically.",
        "Orbit Updater",MB_OK|MB_ICONINFORMATION);

    // wait for screensaver process to die (max 30s)
    for(int i=0;i<60;i++){
        HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
        bool found=false;
        if(snap!=INVALID_HANDLE_VALUE){
            PROCESSENTRY32 pe;pe.dwSize=sizeof(pe);
            if(Process32First(snap,&pe)){
                do{
                    if(_stricmp(pe.szExeFile,"orbit_screensaver.scr")==0||
                       _stricmp(pe.szExeFile,"orbit_screensaver.exe")==0){found=true;break;}
                }while(Process32Next(snap,&pe));
            }
            CloseHandle(snap);
        }
        if(!found) break;
        Sleep(500);
    }

    // fetch latest tag to build download url
    std::string tag=fetchLatestTag();
    if(tag.empty()){
        MessageBoxA(NULL,"Failed to fetch latest version info.\nCheck your internet connection.","Orbit Updater",MB_OK|MB_ICONERROR);
        return 1;
    }

    // download orbit-update.zip
    std::string zipUrl="https://github.com/MalikHw/orbit-screensaver/releases/download/"+tag+"/orbit-update.zip";
    std::string zipPath=exeDir+"\\orbit-update.zip";

    if(!downloadFile(zipUrl.c_str(),zipPath.c_str())){
        MessageBoxA(NULL,"Failed to download update.\nCheck your internet connection.","Orbit Updater",MB_OK|MB_ICONERROR);
        return 1;
    }

    // extract to install dir
    if(!extractZip(zipPath.c_str(),exeDir.c_str())){
        MessageBoxA(NULL,"Failed to extract update.","Orbit Updater",MB_OK|MB_ICONERROR);
        return 1;
    }

    // cleanup zip
    DeleteFileA(zipPath.c_str());

    MessageBoxA(NULL,"Update installed successfully!\n\nReinstall the screensaver by right-clicking orbit_screensaver.scr -> Install.",
        "Orbit Updater",MB_OK|MB_ICONINFORMATION);

    return 0;
}
