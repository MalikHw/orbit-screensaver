// thx github for the api
#include <windows.h>
#include <winhttp.h>
#include <urlmon.h>
#include <shlobj.h>
#include <shldisp.h>
#include <stdio.h>
#include <string.h>
#include <tlhelp32.h>
#include <string>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

static std::string getExeDir() {
    char buf[MAX_PATH]; GetModuleFileNameA(NULL,buf,MAX_PATH);
    std::string s(buf); return s.substr(0,s.rfind('\\'));
}

static bool downloadFile(const char* url, const char* destPath) {
    return URLDownloadToFileA(NULL,url,destPath,0,NULL)==S_OK;
}

static bool extractZip(const char* zipPath, const char* destDir) {
    wchar_t wzip[MAX_PATH], wdest[MAX_PATH];
    MultiByteToWideChar(CP_ACP,0,zipPath,-1,wzip,MAX_PATH);
    MultiByteToWideChar(CP_ACP,0,destDir,-1,wdest,MAX_PATH);

    CoInitialize(NULL);
    bool ok=false;
    IShellDispatch* pShell=nullptr;
    if(SUCCEEDED(CoCreateInstance(CLSID_Shell,NULL,CLSCTX_INPROC_SERVER,IID_IShellDispatch,(void**)&pShell))){
        VARIANT vZip,vDest,vOpts;
        VariantInit(&vZip); VariantInit(&vDest); VariantInit(&vOpts);
        vZip.vt=VT_BSTR;  vZip.bstrVal=SysAllocString(wzip);
        vDest.vt=VT_BSTR; vDest.bstrVal=SysAllocString(wdest);
        vOpts.vt=VT_I4;   vOpts.lVal=4|16|256|1024;

        Folder* pDestFolder=nullptr;
        Folder* pZipFolder=nullptr;
        if(SUCCEEDED(pShell->NameSpace(vDest,&pDestFolder))&&pDestFolder){
            if(SUCCEEDED(pShell->NameSpace(vZip,&pZipFolder))&&pZipFolder){
                FolderItems* pItems=nullptr;
                if(SUCCEEDED(pZipFolder->Items(&pItems))&&pItems){
                    VARIANT vItems; VariantInit(&vItems);
                    vItems.vt=VT_DISPATCH; vItems.pdispVal=pItems;
                    ok=SUCCEEDED(pDestFolder->CopyHere(vItems,vOpts));
                    pItems->Release();
                }
                pZipFolder->Release();
            }
            pDestFolder->Release();
        }
        SysFreeString(vZip.bstrVal);
        SysFreeString(vDest.bstrVal);
        pShell->Release();
        Sleep(2000);
    }
    CoUninitialize();
    return ok;
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

    // wait for screensaver to close
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

    // zip was already downloaded by main.cpp settings UI
    std::string zipPath=exeDir+"\\orbit-update.zip";

    // fallback: if zip not there, download it ourselves
    if(GetFileAttributesA(zipPath.c_str())==INVALID_FILE_ATTRIBUTES){
        std::string tag=fetchLatestTag();
        if(tag.empty()){
            MessageBoxA(NULL,"Failed to fetch latest version info.\nCheck your internet connection.","Orbit Updater",MB_OK|MB_ICONERROR);
            return 1;
        }
        std::string zipUrl="https://github.com/MalikHw/orbit-screensaver/releases/download/"+tag+"/orbit-update.zip";
        if(!downloadFile(zipUrl.c_str(),zipPath.c_str())){
            MessageBoxA(NULL,"Failed to download update.\nCheck your internet connection.","Orbit Updater",MB_OK|MB_ICONERROR);
            return 1;
        }
    }

    // extract to temp folder
    std::string tmpDir=exeDir+"\\orbit-update-tmp";
    CreateDirectoryA(tmpDir.c_str(),NULL);

    if(!extractZip(zipPath.c_str(),tmpDir.c_str())){
        MessageBoxA(NULL,"Failed to extract update.","Orbit Updater",MB_OK|MB_ICONERROR);
        return 1;
    }
    DeleteFileA(zipPath.c_str());

    // copy files to install dir; updater.exe -> updater.exe.pending
    WIN32_FIND_DATAA fd;
    std::string pattern=tmpDir+"\\*";
    HANDLE hFind=FindFirstFileA(pattern.c_str(),&fd);
    if(hFind!=INVALID_HANDLE_VALUE){
        do {
            if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) continue;
            std::string src=tmpDir+"\\"+fd.cFileName;
            std::string dst;
            if(_stricmp(fd.cFileName,"updater.exe")==0)
                dst=exeDir+"\\updater.exe.pending";
            else
                dst=exeDir+"\\"+fd.cFileName;
            MoveFileExA(src.c_str(),dst.c_str(),MOVEFILE_REPLACE_EXISTING);
        } while(FindNextFileA(hFind,&fd));
        FindClose(hFind);
    }
    RemoveDirectoryA(tmpDir.c_str());

    return 0; // silent, no messagebox
}
