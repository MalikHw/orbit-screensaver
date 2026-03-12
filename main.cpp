#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_image.h>
#include <box2d/box2d.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <vector>
#include <string>
// epstein
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commdlg.h>
#include <timeapi.h>
#include <winhttp.h>
#include <urlmon.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "urlmon.lib")
#include <SDL2/SDL_syswm.h>

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl2.h"
#include "logo_data.h"

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

#define BG_BLACK     0
#define BG_COLOR     1
#define BG_IMAGE     2
#define BG_SNAPSHOT  3
#define BG_BLUR_SNAP 4

#define FIT_STRETCH 0
#define FIT_ZOOM    1
#define FIT_TILE    2

#define NUM_ORBS    11
#define PPM         40.0f
#define PLAYER_SIZE 80

struct Settings {
    int   speed;
    int   fps;
    int   bg_mode;
    float bg_color[3];
    char  bg_image[512];
    int   bg_fit;
    char  cube_path[512];
    bool  no_ground;
    float orb_scale;
    int   orb_count;
    bool  auto_update_check;
    bool  auto_update_install;
    int   cube_chance;
};
static Settings g_settings = {
    10, 60, BG_BLACK, {0.12f,0.12f,0.12f}, "", FIT_STRETCH, "",
    false, 1.0f, 120, true, false, 50
};

static std::string getExeDir() {
    char buf[MAX_PATH]; GetModuleFileNameA(NULL,buf,MAX_PATH);
    std::string s(buf); return s.substr(0,s.rfind('\\'));
}
static std::string getCfgPath() {
    return getExeDir()+"\\settings.ini";
}

static void loadCfg() {
    FILE* f=fopen(getCfgPath().c_str(),"r"); if(!f)return;
    char line[640];
    while(fgets(line,sizeof(line),f)){
        int iv; float fv,fv2,fv3; char sv[512];
        if(sscanf(line,"speed=%d",&iv)==1)              g_settings.speed=iv;
        if(sscanf(line,"fps=%d",&iv)==1)                g_settings.fps=iv;
        if(sscanf(line,"bg_mode=%d",&iv)==1)            g_settings.bg_mode=iv;
        if(sscanf(line,"bg_color=%f,%f,%f",&fv,&fv2,&fv3)==3){g_settings.bg_color[0]=fv;g_settings.bg_color[1]=fv2;g_settings.bg_color[2]=fv3;}
        if(sscanf(line,"bg_fit=%d",&iv)==1)             g_settings.bg_fit=iv;
        if(sscanf(line,"no_ground=%d",&iv)==1)          g_settings.no_ground=(iv!=0);
        if(sscanf(line,"orb_scale=%f",&fv)==1)          g_settings.orb_scale=fv;
        if(sscanf(line,"orb_count=%d",&iv)==1)          g_settings.orb_count=iv;
        if(sscanf(line,"auto_update_check=%d",&iv)==1)  g_settings.auto_update_check=(iv!=0);
        if(sscanf(line,"auto_update_install=%d",&iv)==1)g_settings.auto_update_install=(iv!=0);
        if(sscanf(line,"cube_chance=%d",&iv)==1)        g_settings.cube_chance=iv;
        if(sscanf(line,"bg_image=%511[^\n]",sv)==1)     strncpy(g_settings.bg_image,sv,511);
        if(sscanf(line,"cube_path=%511[^\n]",sv)==1)    strncpy(g_settings.cube_path,sv,511);
    }
    fclose(f);
}
static void saveCfg() {
    FILE* f=fopen(getCfgPath().c_str(),"w"); if(!f)return;
    fprintf(f,"speed=%d\n",g_settings.speed);
    fprintf(f,"fps=%d\n",g_settings.fps);
    fprintf(f,"bg_mode=%d\n",g_settings.bg_mode);
    fprintf(f,"bg_color=%f,%f,%f\n",g_settings.bg_color[0],g_settings.bg_color[1],g_settings.bg_color[2]);
    fprintf(f,"bg_fit=%d\n",g_settings.bg_fit);
    fprintf(f,"no_ground=%d\n",(int)g_settings.no_ground);
    fprintf(f,"orb_scale=%f\n",g_settings.orb_scale);
    fprintf(f,"orb_count=%d\n",g_settings.orb_count);
    fprintf(f,"auto_update_check=%d\n",(int)g_settings.auto_update_check);
    fprintf(f,"auto_update_install=%d\n",(int)g_settings.auto_update_install);
    fprintf(f,"cube_chance=%d\n",g_settings.cube_chance);
    fprintf(f,"bg_image=%s\n",g_settings.bg_image);
    fprintf(f,"cube_path=%s\n",g_settings.cube_path);
    fclose(f);
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
    WinHttpAddRequestHeaders(hRequest,L"User-Agent: OrbitScreensaver",-1,WINHTTP_ADDREQ_FLAG_ADD);
    if(WinHttpSendRequest(hRequest,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)
       && WinHttpReceiveResponse(hRequest,NULL)){
        char buf[4096]=""; DWORD read=0;
        WinHttpReadData(hRequest,buf,sizeof(buf)-1,&read);
        buf[read]=0;
        const char* p=strstr(buf,"\"tag_name\":");
        if(p){
            p+=11; while(*p=='"'||*p==' ')p++;
            char tag[64]=""; int i=0;
            while(*p&&*p!='"'&&i<63) tag[i++]=*p++;
            tag[i]=0; result=tag;
        }
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

static void launchUpdater() {
    std::string updaterPath=getExeDir()+"\\updater.exe";
    ShellExecuteA(NULL,"open",updaterPath.c_str(),NULL,NULL,SW_SHOW);
}

struct UpdateDownloadState {
    volatile float progress;
    volatile int   done; // 0=running, 1=ok, -1=fail
    std::string    url;
    std::string    destPath;
};
struct UpdateCallback : public IBindStatusCallback {
    UpdateDownloadState* s;
    UpdateCallback(UpdateDownloadState* s):s(s){}
    HRESULT STDMETHODCALLTYPE OnProgress(ULONG prog,ULONG progMax,ULONG,LPCWSTR) override {
        if(progMax>0) s->progress=(float)prog/(float)progMax; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnStartBinding(DWORD,IBinding*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetPriority(LONG*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE OnLowResource(DWORD) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE OnStopBinding(HRESULT,LPCWSTR) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetBindInfo(DWORD*,BINDINFO*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE OnDataAvailable(DWORD,DWORD,FORMATETC*,STGMEDIUM*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE OnObjectAvailable(REFIID,IUnknown*) override{return E_NOTIMPL;}
    ULONG STDMETHODCALLTYPE AddRef() override{return 1;}
    ULONG STDMETHODCALLTYPE Release() override{return 1;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override{return E_NOINTERFACE;}
};
static DWORD WINAPI updateDownloadThread(void* param){
    UpdateDownloadState* s=(UpdateDownloadState*)param;
    UpdateCallback cb(s);
    HRESULT hr=URLDownloadToFileA(NULL,s->url.c_str(),s->destPath.c_str(),0,&cb);
    s->done=(hr==S_OK)?1:-1;
    return 0;
}
static UpdateDownloadState* g_updateDL=nullptr;

struct TagCheckState { volatile int done; char tag[64]; };
static TagCheckState g_tagCheck={0,""};
static DWORD WINAPI tagCheckThread(void*){
    std::string t=fetchLatestTag();
    strncpy((char*)g_tagCheck.tag,t.c_str(),63);
    g_tagCheck.done=1;
    return 0;
}

static unsigned char* captureDesktop(int* outW, int* outH) {
    int W=GetSystemMetrics(SM_CXSCREEN), H=GetSystemMetrics(SM_CYSCREEN);
    *outW=W; *outH=H;
    HDESK hDesk=OpenDesktopA("Default",0,FALSE,GENERIC_READ);
    HDC screenDC=GetDC(NULL);
    if(hDesk){ SetThreadDesktop(hDesk); ReleaseDC(NULL,screenDC); screenDC=GetDC(NULL); }
    HDC memDC=CreateCompatibleDC(screenDC);
    BITMAPINFO bmi={}; bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth=W; bmi.bmiHeader.biHeight=-H;
    bmi.bmiHeader.biPlanes=1; bmi.bmiHeader.biBitCount=32;
    bmi.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;
    HBITMAP bmp=CreateDIBSection(screenDC,&bmi,DIB_RGB_COLORS,&bits,NULL,0);
    HBITMAP old=(HBITMAP)SelectObject(memDC,bmp);
    BitBlt(memDC,0,0,W,H,screenDC,0,0,SRCCOPY);
    SelectObject(memDC,old);
    unsigned char* pixels=(unsigned char*)malloc(W*H*4);
    unsigned char* src2=(unsigned char*)bits;
    for(int i=0;i<W*H;i++){pixels[i*4+0]=src2[i*4+2];pixels[i*4+1]=src2[i*4+1];pixels[i*4+2]=src2[i*4+0];pixels[i*4+3]=255;}
    DeleteObject(bmp); DeleteDC(memDC); ReleaseDC(NULL,screenDC);
    if(hDesk) CloseDesktop(hDesk);
    return pixels;
}
// this blur runs on the CPU like an idiot, don't touch it
static void boxBlur(unsigned char* pixels, int W, int H, int radius) {
    unsigned char* tmp=(unsigned char*)malloc(W*H*4);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int r=0,g=0,b=0,cnt=0;
        for(int k=-radius;k<=radius;k++){int nx=x+k;if(nx<0||nx>=W)continue;int idx=(y*W+nx)*4;r+=pixels[idx];g+=pixels[idx+1];b+=pixels[idx+2];cnt++;}
        int idx=(y*W+x)*4;tmp[idx]=r/cnt;tmp[idx+1]=g/cnt;tmp[idx+2]=b/cnt;tmp[idx+3]=255;
    }
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int r=0,g=0,b=0,cnt=0;
        for(int k=-radius;k<=radius;k++){int ny=y+k;if(ny<0||ny>=H)continue;int idx=(ny*W+x)*4;r+=tmp[idx];g+=tmp[idx+1];b+=tmp[idx+2];cnt++;}
        int idx=(y*W+x)*4;pixels[idx]=r/cnt;pixels[idx+1]=g/cnt;pixels[idx+2]=b/cnt;pixels[idx+3]=255;
    }
    free(tmp);
}

struct Texture { GLuint id; int w,h; bool ok; };
static Texture loadTexture(const char* path) {
    Texture t={0,0,0,false};
    SDL_Surface* surf=IMG_Load(path); if(!surf)return t;
    SDL_Surface* conv=SDL_ConvertSurfaceFormat(surf,SDL_PIXELFORMAT_RGBA32,0);
    SDL_FreeSurface(surf); if(!conv)return t;
    glGenTextures(1,&t.id);
    glBindTexture(GL_TEXTURE_2D,t.id);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,conv->w,conv->h,0,GL_RGBA,GL_UNSIGNED_BYTE,conv->pixels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    t.w=conv->w;t.h=conv->h;t.ok=true;SDL_FreeSurface(conv);return t;
}
static Texture loadTextureFromPixels(unsigned char* pixels, int w, int h) {
    Texture t={0,0,0,false};
    glGenTextures(1,&t.id);glBindTexture(GL_TEXTURE_2D,t.id);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    t.w=w;t.h=h;t.ok=true;return t;
}
static void drawTexturedQuad(GLuint texId,float cx,float cy,float w,float h,float angleDeg){
    glEnable(GL_TEXTURE_2D);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D,texId);
    glPushMatrix();glTranslatef(cx,cy,0);glRotatef(-angleDeg,0,0,1);
    float hw=w/2,hh=h/2;
    glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(-hw,-hh);glTexCoord2f(1,0);glVertex2f(hw,-hh);glTexCoord2f(1,1);glVertex2f(hw,hh);glTexCoord2f(0,1);glVertex2f(-hw,hh);glEnd();
    glPopMatrix();glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);
}
static void drawBgTex(Texture& bg, int W, int H) {
    if(!bg.ok)return;
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,bg.id);glColor4f(1,1,1,1);
    if(g_settings.bg_fit==FIT_ZOOM){
        float sx=(float)W/bg.w,sy=(float)H/bg.h,sc=fmaxf(sx,sy);
        float dw=bg.w*sc,dh=bg.h*sc,ox=(W-dw)/2,oy=(H-dh)/2;
        glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(ox,oy);glTexCoord2f(1,0);glVertex2f(ox+dw,oy);glTexCoord2f(1,1);glVertex2f(ox+dw,oy+dh);glTexCoord2f(0,1);glVertex2f(ox,oy+dh);glEnd();
    } else if(g_settings.bg_fit==FIT_TILE){
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        float tx=(float)W/bg.w,ty=(float)H/bg.h;
        glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(0,0);glTexCoord2f(tx,0);glVertex2f(W,0);glTexCoord2f(tx,ty);glVertex2f(W,H);glTexCoord2f(0,ty);glVertex2f(0,H);glEnd();
    } else {
        glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(0,0);glTexCoord2f(1,0);glVertex2f(W,0);glTexCoord2f(1,1);glVertex2f(W,H);glTexCoord2f(0,1);glVertex2f(0,H);glEnd();
    }
    glDisable(GL_TEXTURE_2D);
}
static void drawCircleFallback(float cx,float cy,float r){
    glColor3f(0.39f,0.39f,0.78f);
    glBegin(GL_TRIANGLE_FAN);glVertex2f(cx,cy);
    for(int i=0;i<=32;i++){float a=i*2*(float)M_PI/32;glVertex2f(cx+cosf(a)*r,cy+sinf(a)*r);}
    glEnd();glColor3f(1,1,1);
}

struct Ball { b2Body* body; float radius; int orbIdx; bool isPlayer; };

struct MesaDownloadState {
    volatile float progress;
    volatile int   done; // 0=running, 1=ok, -1=fail
    std::string    url;
    std::string    destPath;
};
struct MesaCallback : public IBindStatusCallback {
    MesaDownloadState* s;
    MesaCallback(MesaDownloadState* s):s(s){}
    HRESULT STDMETHODCALLTYPE OnProgress(ULONG prog,ULONG progMax,ULONG,LPCWSTR) override {
        if(progMax>0) s->progress=(float)prog/(float)progMax; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnStartBinding(DWORD,IBinding*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetPriority(LONG*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE OnLowResource(DWORD) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE OnStopBinding(HRESULT,LPCWSTR) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetBindInfo(DWORD*,BINDINFO*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE OnDataAvailable(DWORD,DWORD,FORMATETC*,STGMEDIUM*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE OnObjectAvailable(REFIID,IUnknown*) override{return E_NOTIMPL;}
    ULONG STDMETHODCALLTYPE AddRef() override{return 1;}
    ULONG STDMETHODCALLTYPE Release() override{return 1;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override{return E_NOINTERFACE;}
};
static DWORD WINAPI mesaThread(void* param){
    MesaDownloadState* s=(MesaDownloadState*)param;
    MesaCallback cb(s);
    HRESULT hr=URLDownloadToFileA(NULL,s->url.c_str(),s->destPath.c_str(),0,&cb);
    s->done=(hr==S_OK)?1:-1;
    return 0;
}
static MesaDownloadState* g_mesaDL=nullptr;

static void startMesaDownload() {
    const char* url = "https://github.com/MalikHw/orbit-screensaver/releases/download/mesa3d/opengl32.dll";
    g_mesaDL=new MesaDownloadState();
    g_mesaDL->progress=0.0f;
    g_mesaDL->done=0;
    g_mesaDL->url=url;
    g_mesaDL->destPath=getExeDir()+"\\opengl32.dll";
    CreateThread(NULL,0,mesaThread,g_mesaDL,0,NULL);
}

static bool g_preview_clicked = false;

static bool runImGuiSettings() {
    if(SDL_Init(SDL_INIT_VIDEO)<0) return false;
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,0);
    SDL_Window* win=SDL_CreateWindow("Orbit Screensaver - Settings",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,460,560,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    SDL_GLContext ctx=SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io=ImGui::GetIO(); io.IniFilename=nullptr;
    if(!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf",16.0f))
        if(!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\cour.ttf",16.0f))
            io.Fonts->AddFontDefault();

    ImGui::StyleColorsDark();
    ImGuiStyle& style=ImGui::GetStyle();
    style.WindowRounding=6;style.FrameRounding=4;style.GrabRounding=4;
    style.Colors[ImGuiCol_Button]       =ImVec4(0.26f,0.59f,0.98f,0.5f);
    style.Colors[ImGuiCol_ButtonHovered]=ImVec4(0.26f,0.59f,0.98f,0.8f);

    ImGui_ImplSDL2_InitForOpenGL(win,ctx);
    ImGui_ImplOpenGL2_Init();

    GLuint logoTex=0;
    {
        SDL_RWops* rw=SDL_RWFromConstMem(logo_png,logo_png_len);
        SDL_Surface* surf=IMG_LoadPNG_RW(rw);
        SDL_RWclose(rw);
        if(surf){
            SDL_Surface* conv=SDL_ConvertSurfaceFormat(surf,SDL_PIXELFORMAT_RGBA32,0);
            SDL_FreeSurface(surf);
            if(conv){
                glGenTextures(1,&logoTex);
                glBindTexture(GL_TEXTURE_2D,logoTex);
                glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,conv->w,conv->h,0,GL_RGBA,GL_UNSIGNED_BYTE,conv->pixels);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                SDL_FreeSurface(conv);
            }
        }
    }

    const char* bgNames[]={"Black","Custom Color","Image","Transparent (snapshot)","Blur (snapshot)"};
    const char* fitNames[]={"Stretch","Zoom","Tile"};

    static std::string latestTag="";
    static bool updateChecked=false;
    static bool checkingNow=false;

    bool running=true;
    g_preview_clicked=false;

    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if(ev.type==SDL_QUIT) running=false;
        }
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        int W,H; SDL_GetWindowSize(win,&W,&H);
        ImGui::SetNextWindowPos(ImVec2(0,0));
        ImGui::SetNextWindowSize(ImVec2((float)W,(float)H));
        ImGui::Begin("##main",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove);

        ImGui::TextColored(ImVec4(0.4f,0.8f,1.0f,1.0f),"ORBIT SCREENSAVER");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1.0f),"v%s",APP_VERSION);
        ImGui::Separator(); ImGui::Spacing();

        ImGui::SliderInt("Speed",&g_settings.speed,1,20);

        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("FPS",&g_settings.fps,0);
        if(g_settings.fps<1)g_settings.fps=1;if(g_settings.fps>500)g_settings.fps=500;
        ImGui::SameLine();
        int fpsP[]={30,60,120,144,240,500};
        for(int fp:fpsP){char l[8];sprintf(l,"%d",fp);if(ImGui::SmallButton(l))g_settings.fps=fp;ImGui::SameLine();}
        ImGui::NewLine(); ImGui::Spacing();

        static bool broPopupPending=false;
        static bool wasOrbFieldFocused=false;
        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("Orb count",&g_settings.orb_count,1);
        if(g_settings.orb_count<1)g_settings.orb_count=1;
        bool orbFieldFocused=ImGui::IsItemActive();
        if(wasOrbFieldFocused && !orbFieldFocused && g_settings.orb_count < 20)
            broPopupPending=true;
        wasOrbFieldFocused=orbFieldFocused;
        ImGui::SameLine();
        if(ImGui::SmallButton("Low"))  { g_settings.orb_count=30; }  ImGui::SameLine();
        if(ImGui::SmallButton("Med"))  { g_settings.orb_count=80; }  ImGui::SameLine();
        if(ImGui::SmallButton("High")) { g_settings.orb_count=120; } ImGui::SameLine();
        if(ImGui::SmallButton("Giga")) { g_settings.orb_count=210; }
        if(broPopupPending){ ImGui::OpenPopup("bro"); broPopupPending=false; }

        if(ImGui::BeginPopupModal("bro",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::Text("bro what the fuck? :sob:, why is even THAT!?");
            if(ImGui::Button("yes")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::SetNextItemWidth(180);
        ImGui::SliderFloat("Orb size",&g_settings.orb_scale,0.3f,3.0f);
        ImGui::SameLine();
        if(ImGui::SmallButton("S")) g_settings.orb_scale=0.5f; ImGui::SameLine();
        if(ImGui::SmallButton("N")) g_settings.orb_scale=1.0f; ImGui::SameLine();
        if(ImGui::SmallButton("L")) g_settings.orb_scale=1.5f;

        ImGui::SetNextItemWidth(180);
        ImGui::SliderInt("Cube chance",&g_settings.cube_chance,0,100);
        ImGui::SameLine(); ImGui::TextDisabled("%%");
        ImGui::Spacing();

        ImGui::Text("Cube PNG");
        ImGui::SetNextItemWidth(280);
        ImGui::InputText("##cube",g_settings.cube_path,sizeof(g_settings.cube_path));
        ImGui::SameLine();
        if(ImGui::Button("Browse##cube")){
            OPENFILENAMEA ofn={};char buf[512]="";
            ofn.lStructSize=sizeof(ofn);ofn.lpstrFilter="PNG\0*.png\0All\0*.*\0";
            ofn.lpstrFile=buf;ofn.nMaxFile=sizeof(buf);ofn.Flags=OFN_FILEMUSTEXIST;
            if(GetOpenFileNameA(&ofn)) strncpy(g_settings.cube_path,buf,511);
        }
        ImGui::Spacing();

        ImGui::Text("Background");
        ImGui::SetNextItemWidth(200);
        ImGui::Combo("##bg",&g_settings.bg_mode,bgNames,5);
        if(g_settings.bg_mode==BG_COLOR)
            ImGui::ColorEdit3("Color",&g_settings.bg_color[0]);
        if(g_settings.bg_mode==BG_IMAGE){
            ImGui::SetNextItemWidth(280);
            ImGui::InputText("##img",g_settings.bg_image,sizeof(g_settings.bg_image));
            ImGui::SameLine();
            if(ImGui::Button("Browse##img")){
                OPENFILENAMEA ofn={};char buf[512]="";
                ofn.lStructSize=sizeof(ofn);ofn.lpstrFilter="Images\0*.png;*.jpg;*.bmp\0All\0*.*\0";
                ofn.lpstrFile=buf;ofn.nMaxFile=sizeof(buf);ofn.Flags=OFN_FILEMUSTEXIST;
                if(GetOpenFileNameA(&ofn)) strncpy(g_settings.bg_image,buf,511);
            }
            ImGui::SetNextItemWidth(120);
            ImGui::Combo("Fit",&g_settings.bg_fit,fitNames,3);
        }
        ImGui::Spacing();

        ImGui::Checkbox("No ground (infinite fall)",&g_settings.no_ground);
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.4f,0.8f,1.0f,1.0f),"Updates");
        ImGui::Spacing();
        if(g_updateDL && g_updateDL->done==0){
            char lbl[16]; snprintf(lbl,sizeof(lbl),"%.0f%%",g_updateDL->progress*100.0f);
            ImGui::ProgressBar(g_updateDL->progress,ImVec2(220,20),lbl);
            ImGui::SameLine(); ImGui::TextColored(ImVec4(1,1,0,1),"Downloading update...");
        } else if(g_updateDL && g_updateDL->done==1){
            ImGui::TextColored(ImVec4(0,1,0,1),"Downloaded! Launching updater...");
            saveCfg(); launchUpdater(); running=false;
        } else if(g_updateDL && g_updateDL->done==-1){
            ImGui::TextColored(ImVec4(1,0.3f,0.3f,1),"Download failed!");
        } else {
            if(checkingNow){
                ImGui::TextColored(ImVec4(1,1,0,1),"Checking...");
            } else if(!updateChecked){
                if(ImGui::Button("Check for updates",ImVec2(200,24))){
                    checkingNow=true;
                    latestTag=fetchLatestTag();
                    updateChecked=true;
                    checkingNow=false;
                }
            } else if(latestTag.empty()||latestTag==APP_VERSION){
                ImGui::TextColored(ImVec4(0,1,0,1),"You're up to date! (%s)",APP_VERSION);
            } else {
                ImGui::TextColored(ImVec4(1,0.5f,0,1),"Update available: %s",latestTag.c_str());
                ImGui::SameLine();
                if(ImGui::Button("Install")){
                    g_updateDL=new UpdateDownloadState();
                    g_updateDL->progress=0.0f;
                    g_updateDL->done=0;
                    g_updateDL->url="https://github.com/MalikHw/orbit-screensaver/releases/download/"+latestTag+"/orbit-update.zip";
                    g_updateDL->destPath=getExeDir()+"\\orbit-update.zip";
                    CreateThread(NULL,0,updateDownloadThread,g_updateDL,0,NULL);
                }
            }
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        if(ImGui::Button("Save",ImVec2(100,30))){saveCfg();ImGui::OpenPopup("Saved");}
        ImGui::SameLine();
        if(ImGui::Button("Save and Exit",ImVec2(120,30))){saveCfg();g_preview_clicked=false;running=false;}
        if(ImGui::BeginPopupModal("Saved",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::Text("Settings saved!");
            if(ImGui::Button("OK")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        if(g_mesaDL && g_mesaDL->done==0){
            char lbl[16]; snprintf(lbl,sizeof(lbl),"%.0f%%",g_mesaDL->progress*100.0f);
            ImGui::ProgressBar(g_mesaDL->progress,ImVec2(180,20),lbl);
            ImGui::SameLine(); ImGui::TextColored(ImVec4(1,1,0,1),"Downloading...");
        } else if(g_mesaDL && g_mesaDL->done==1){
            ImGui::TextColored(ImVec4(0,1,0,1),"Mesa3D installed!");
        } else if(g_mesaDL && g_mesaDL->done==-1){
            ImGui::TextColored(ImVec4(1,0.3f,0.3f,1),"Download failed!");
            ImGui::SameLine();
            if(ImGui::SmallButton("Retry")){ delete g_mesaDL; g_mesaDL=nullptr; startMesaDownload(); }
        } else {
            if(ImGui::Button("Install Mesa3D",ImVec2(180,24))) ImGui::OpenPopup("mesa_confirm");
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("Software OpenGL renderer - only if you get a white square!");
        }
        if(ImGui::BeginPopupModal("mesa_confirm",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::TextColored(ImVec4(1,0.8f,0,1),"Only use this if you get a white square,");
            ImGui::Text("or you don't want GPU usage.");
            ImGui::Spacing();
            if(ImGui::Button("Download",ImVec2(100,24))){ startMesaDownload(); ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if(ImGui::Button("Cancel",ImVec2(80,24))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1.0f),"by MalikHw47");
        ImGui::Spacing();
        if(ImGui::SmallButton("MalikHw47")) ShellExecuteA(0,"open","https://malikhw.github.io",0,0,SW_SHOW);
        ImGui::SameLine();ImGui::Text("-");ImGui::SameLine();
        if(ImGui::SmallButton("youtube"))   ShellExecuteA(0,"open","https://youtube.com/@MalikHw47",0,0,SW_SHOW);
        ImGui::SameLine();ImGui::Text("-");ImGui::SameLine();
        if(ImGui::SmallButton("github"))    ShellExecuteA(0,"open","https://github.com/MalikHw",0,0,SW_SHOW);
        ImGui::SameLine();ImGui::Text("-");ImGui::SameLine();
        if(ImGui::SmallButton("twitch"))    ShellExecuteA(0,"open","https://twitch.tv/MalikHw47",0,0,SW_SHOW);
        ImGui::Spacing();
        if(ImGui::Button("Join my server",ImVec2(180,22)))   ShellExecuteA(0,"open","https://discord.gg/G9bZ92eg2n",0,0,SW_SHOW);
        ImGui::SameLine();
        if(ImGui::Button("Get me a gift!",ImVec2(150,22)))   ShellExecuteA(0,"open","https://throne.com/MalikHw47",0,0,SW_SHOW);
        if(ImGui::Button("Get me MegaHack!",ImVec2(180,22))) ShellExecuteA(0,"open","https://absolllute.com/store/mega_hack?gift=1",0,0,SW_SHOW);
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("My discord is MalikHw btw");
        ImGui::SameLine();
        if(ImGui::Button("Donate!",ImVec2(150,22)))           ShellExecuteA(0,"open","https://ko-fi.com/malikhw47",0,0,SW_SHOW);

        if(logoTex){
            const float logoSize=56.0f;
            ImVec2 winPos=ImGui::GetWindowPos();
            ImVec2 winSize=ImGui::GetWindowSize();
            ImVec2 logoPos=ImVec2(winPos.x+winSize.x-logoSize-6, winPos.y+winSize.y-logoSize-6);
            ImGui::GetWindowDrawList()->AddImage(
                (ImTextureID)(uintptr_t)logoTex,
                logoPos,
                ImVec2(logoPos.x+logoSize, logoPos.y+logoSize),
                ImVec2(0,0), ImVec2(1,1),
                IM_COL32(255,255,255,180));
        }

        ImGui::End();
        ImGui::Render();
        glViewport(0,0,W,H);
        glClearColor(0.1f,0.1f,0.1f,1);glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(win);
    }

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    if(logoTex) glDeleteTextures(1,&logoTex);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return g_preview_clicked;
}

static void runScreensaver(bool isPreview, void* previewHandle) {
    HWND parentHwnd=(HWND)previewHandle;
    if(isPreview&&parentHwnd){
        char e[128];
        sprintf(e,"SDL_VIDEODRIVER=windib");putenv(e);
        sprintf(e,"SDL_WINDOWID=%llu",(unsigned long long)(uintptr_t)parentHwnd);putenv(e);
    }

    bool needSnap=!isPreview&&(g_settings.bg_mode==BG_SNAPSHOT||g_settings.bg_mode==BG_BLUR_SNAP);
    unsigned char* snapPixels=nullptr; int snapW=0,snapH=0;
    if(needSnap){
        snapPixels=captureDesktop(&snapW,&snapH);
        if(g_settings.bg_mode==BG_BLUR_SNAP) boxBlur(snapPixels,snapW,snapH,12);
    }

    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0){if(snapPixels)free(snapPixels);return;}
    IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);

    int W,H;
    if(isPreview){RECT rc;GetClientRect(parentHwnd,&rc);W=rc.right-rc.left;if(W<=0)W=152;H=rc.bottom-rc.top;if(H<=0)H=112;}
    else{SDL_DisplayMode dm;SDL_GetCurrentDisplayMode(0,&dm);W=dm.w;H=dm.h;}

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,0);

    SDL_Window* win=isPreview
        ?SDL_CreateWindow("orbit",0,0,W,H,SDL_WINDOW_OPENGL)
        :SDL_CreateWindow("orbit",0,0,W,H,SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN_DESKTOP|SDL_WINDOW_BORDERLESS);
    if(!isPreview) SDL_ShowCursor(SDL_DISABLE);
    if(!win){SDL_Quit();return;}

    SDL_GLContext ctx=SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,W,H,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable(GL_DEPTH_TEST);

    Texture snapTex={0,0,0,false};
    if(needSnap&&snapPixels){snapTex=loadTextureFromPixels(snapPixels,snapW,snapH);free(snapPixels);snapPixels=nullptr;}

    std::string assetDir=getExeDir();
    Texture orbTex[NUM_ORBS];
    for(int i=0;i<NUM_ORBS;i++){char p[600];snprintf(p,sizeof(p),"%s/orb%d.png",assetDir.c_str(),i+1);orbTex[i]=loadTexture(p);}
    Texture cubeTex={0,0,0,false};
    {const char* cs=g_settings.cube_path[0]?g_settings.cube_path:nullptr;
     if(!cs){char p[600];snprintf(p,sizeof(p),"%s/cube.png",assetDir.c_str());cubeTex=loadTexture(p);}
     else cubeTex=loadTexture(cs);}
    Texture bgTex={0,0,0,false};
    if(g_settings.bg_mode==BG_IMAGE&&g_settings.bg_image[0]) bgTex=loadTexture(g_settings.bg_image);

    srand((unsigned)time(nullptr));
    bool running=true;

    while(running){
        srand((unsigned)rand());
        int fps=g_settings.fps;if(fps<1)fps=1;if(fps>500)fps=500;
        float speedMult=g_settings.speed/10.0f;
        int numBalls=g_settings.orb_count;if(numBalls<1)numBalls=1;
        b2Vec2 gravity(0.0f,9.8f*speedMult*3.0f);
        b2World world(gravity);
        int dropTime=(int)(20.0f/speedMult);if(dropTime<1)dropTime=1;

        auto makeWall=[&](float x1,float y1,float x2,float y2){
            b2BodyDef bd;bd.type=b2_staticBody;b2Body* b=world.CreateBody(&bd);
            b2EdgeShape es;es.SetTwoSided(b2Vec2(x1/PPM,y1/PPM),b2Vec2(x2/PPM,y2/PPM));
            b2FixtureDef fd;fd.shape=&es;fd.restitution=0.5f;fd.friction=0.7f;
            b->CreateFixture(&fd);return b;
        };
        makeWall(0,0,0,H);makeWall(W,0,W,H);
        b2Body* wallBottom=nullptr;
        if(!g_settings.no_ground) wallBottom=makeWall(0,H,W,H);

        std::vector<Ball> balls;
        int globalTime=0;bool fillingDone=false,draining=false;
        int nextSpawn=0;
        bool playerSpawned=false;
        Uint32 allSpawnedAt=0;
        SDL_Point lastMouse;SDL_GetMouseState(&lastMouse.x,&lastMouse.y);
        int grace=60;
        Uint32 lastTick=SDL_GetTicks();float physAccum=0;const float physStep=1.0f/fps;
        bool simRunning=true;

        while(simRunning&&running){
            globalTime++;
            SDL_Event ev;
            while(SDL_PollEvent(&ev)){
                if(ev.type==SDL_QUIT){running=false;simRunning=false;}
                if(globalTime>grace&&!isPreview){
                    if(ev.type==SDL_KEYDOWN&&ev.key.keysym.sym!=SDLK_PRINTSCREEN){running=false;simRunning=false;}
                    if(ev.type==SDL_MOUSEBUTTONDOWN){running=false;simRunning=false;}
                    if(ev.type==SDL_MOUSEMOTION&&(ev.motion.x!=lastMouse.x||ev.motion.y!=lastMouse.y)){running=false;simRunning=false;}
                }
            }
            if(isPreview&&parentHwnd&&!IsWindow(parentHwnd)){running=false;simRunning=false;}

            while(nextSpawn < numBalls && globalTime >= dropTime * nextSpawn){
                float radius=(40+rand()%20)*g_settings.orb_scale;
                int chosenOrb = rand()%NUM_ORBS; 

                b2BodyDef bd;bd.type=b2_dynamicBody;
                bd.position.Set(((float)W*0.8f/numBalls*(1+rand()%(numBalls*2)))/PPM,-250.0f/PPM);

                bd.angle = (float)(rand() % 360) * ((float)M_PI / 180.0f); 
                
                b2Body* body=world.CreateBody(&bd);
                
                b2FixtureDef fd;
                fd.density=1.0f; fd.restitution=0.5f; fd.friction=1.0f;

                b2CircleShape cs;
                b2PolygonShape ps;

                if (chosenOrb == 10) {
                    ps.SetAsBox(radius/PPM, radius/PPM);
                    fd.shape=&ps;
                } else {
                    cs.m_radius=radius/PPM;
                    fd.shape=&cs;
                }

                body->CreateFixture(&fd);
                body->ApplyLinearImpulse(b2Vec2((10-rand()%21)*0.05f,0),body->GetWorldCenter(),true);
                
                Ball ball; ball.body=body; ball.radius=radius; ball.orbIdx=chosenOrb; ball.isPlayer=false;
                balls.push_back(ball);
                nextSpawn++;
            }
            if(!playerSpawned && nextSpawn >= numBalls/2){
                playerSpawned=true;
                if((rand()%100) < g_settings.cube_chance){
                    float cubeW = PLAYER_SIZE * g_settings.orb_scale;
                    float cubeH = PLAYER_SIZE * g_settings.orb_scale;
                    
                    if (cubeTex.ok) {
                        float tw = (float)cubeTex.w;
                        float th = (float)cubeTex.h;
                        float max_dim = fmaxf(tw, th);
                        cubeW *= (tw / max_dim);
                        cubeH *= (th / max_dim);
                    }

                    b2BodyDef bd;bd.type=b2_dynamicBody;
                    float randomX = (float)(rand() % W);
                    float randomY = - (float)(200 + rand() % 800);
                    bd.position.Set(randomX / PPM, randomY / PPM);

                    bd.angle = (float)(rand() % 360) * ((float)M_PI / 180.0f);
                    
                    b2Body* body=world.CreateBody(&bd);
                    b2PolygonShape ps;
                    ps.SetAsBox((cubeW * 0.5f)/PPM, (cubeH * 0.5f)/PPM);
                    
                    b2FixtureDef fd;
                    fd.shape=&ps;
                    fd.density=1.0f;       
                    fd.restitution=0.5f;   
                    fd.friction=0.7f;      
                    
                    body->CreateFixture(&fd);
                    Ball ball;ball.body=body;ball.radius=PLAYER_SIZE*0.5f*g_settings.orb_scale;ball.orbIdx=0;ball.isPlayer=true;
                    balls.push_back(ball);
                }
            }

            if(!g_settings.no_ground&&!fillingDone && nextSpawn>=numBalls){
                if(allSpawnedAt==0) allSpawnedAt=SDL_GetTicks();
                Uint32 delay=5000+(rand()%1001);
                if(SDL_GetTicks()-allSpawnedAt >= delay){
                    fillingDone=true;
                    draining=true;
                    if(wallBottom){ world.DestroyBody(wallBottom); wallBottom=nullptr; } // bye bitch
                }
            }
            if(!g_settings.no_ground&&draining){
                bool allOff=true;
                for(auto& b:balls)
                    if(b.body->GetPosition().y*PPM < H+300){ allOff=false; break; }
                if(allOff) simRunning=false;
            }
            if(g_settings.no_ground&&globalTime>numBalls*dropTime+500)simRunning=false;

            Uint32 now=SDL_GetTicks();
            physAccum+=(now-lastTick)/1000.0f;lastTick=now;
            while(physAccum>=physStep){world.Step(physStep,8,3);physAccum-=physStep;}

            int bm=g_settings.bg_mode;
            if((bm==BG_SNAPSHOT||bm==BG_BLUR_SNAP)&&snapTex.ok){
                glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
                glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,snapTex.id);glColor4f(1,1,1,1);
                glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(0,0);glTexCoord2f(1,0);glVertex2f(W,0);glTexCoord2f(1,1);glVertex2f(W,H);glTexCoord2f(0,1);glVertex2f(0,H);glEnd();
                glDisable(GL_TEXTURE_2D);
            } else if(bm==BG_IMAGE&&bgTex.ok){
                glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);drawBgTex(bgTex,W,H);
            } else if(bm==BG_COLOR){
                glClearColor(g_settings.bg_color[0],g_settings.bg_color[1],g_settings.bg_color[2],1);glClear(GL_COLOR_BUFFER_BIT);
            } else {
                glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
            }

            for(auto& b:balls){
                float px=b.body->GetPosition().x*PPM,py=b.body->GetPosition().y*PPM;
                float ang=b.body->GetAngle()*180.0f/(float)M_PI;
                if(b.isPlayer){
                    float s=PLAYER_SIZE*g_settings.orb_scale;
                    if(cubeTex.ok) {
                        float tw = (float)cubeTex.w;
                        float th = (float)cubeTex.h;
                        float max_dim = fmaxf(tw, th);
                        float draw_w = s * (tw / max_dim);
                        float draw_h = s * (th / max_dim);
                        drawTexturedQuad(cubeTex.id, px, py, draw_w, draw_h, ang);
                    }
                    else{
                        glColor3f(0.78f,0.39f,0.39f);glPushMatrix();glTranslatef(px,py,0);glRotatef(-ang,0,0,1);float h2=s/2;glBegin(GL_QUADS);glVertex2f(-h2,-h2);glVertex2f(h2,-h2);glVertex2f(h2,h2);glVertex2f(-h2,h2);glEnd();glPopMatrix();glColor3f(1,1,1);
                    }
                } else {
                    float d=b.radius*2;
                    if(orbTex[b.orbIdx].ok) {
                        float tw = (float)orbTex[b.orbIdx].w;
                        float th = (float)orbTex[b.orbIdx].h;
                        float max_dim = fmaxf(tw, th);
                        float draw_w = d * (tw / max_dim);
                        float draw_h = d * (th / max_dim);
                        
                        drawTexturedQuad(orbTex[b.orbIdx].id, px, py, draw_w, draw_h, ang);
                    }
                    else drawCircleFallback(px,py,b.radius);
                }
            }

            SDL_GL_SwapWindow(win);
            Uint32 elapsed=SDL_GetTicks()-now;
            Uint32 target=1000/fps;
            if(elapsed<target)SDL_Delay(target-elapsed);
        }
        balls.clear();
    }

    for(int i=0;i<NUM_ORBS;i++) if(orbTex[i].ok)glDeleteTextures(1,&orbTex[i].id);
    if(cubeTex.ok)glDeleteTextures(1,&cubeTex.id);
    if(bgTex.ok)glDeleteTextures(1,&bgTex.id);
    if(snapTex.ok)glDeleteTextures(1,&snapTex.id);
    SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(win);
    IMG_Quit();SDL_Quit();
}

int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int){
    timeBeginPeriod(1);
    loadCfg();

    // swap pending updater if exists (installed by previous update)
    {
        std::string exeDir=getExeDir();
        std::string pending=exeDir+"\\updater.exe.pending";
        std::string real=exeDir+"\\updater.exe";
        if(GetFileAttributesA(pending.c_str())!=INVALID_FILE_ATTRIBUTES){
            DeleteFileA(real.c_str());
            MoveFileA(pending.c_str(),real.c_str());
        }
    }

    int argc;LPWSTR* wargv=CommandLineToArgvW(GetCommandLineW(),&argc);
    bool doConfig=false,doPreview=false,doRun=false;
    HWND previewHwnd=nullptr;
    for(int i=1;i<argc;i++){
        char a[64];WideCharToMultiByte(CP_ACP,0,wargv[i],-1,a,sizeof(a),0,0);
        for(char* p=a;*p;p++) *p=tolower(*p);
        if(!strncmp(a,"/c",2)||!strncmp(a,"-c",2)) doConfig=true;
        else if(!strncmp(a,"/p",2)||!strncmp(a,"-p",2)){
            doPreview=true;
            const char* colon=strchr(a,':');
            if(colon&&*(colon+1)) previewHwnd=(HWND)(uintptr_t)atoll(colon+1);
            else if(i+1<argc){char b[32];WideCharToMultiByte(CP_ACP,0,wargv[i+1],-1,b,sizeof(b),0,0);previewHwnd=(HWND)(uintptr_t)atoll(b);i++;}
        }
        else if(!strncmp(a,"/s",2)||!strncmp(a,"-s",2)) doRun=true;
    }
    LocalFree(wargv);

    // no args at all = right-click Configure
    if(!doConfig&&!doPreview&&!doRun) doConfig=true;

    if(doConfig) runImGuiSettings();
    else if(doPreview) runScreensaver(true,(void*)previewHwnd);
    else runScreensaver(false,nullptr);

    timeEndPeriod(1);return 0;
}
