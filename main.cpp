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

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commdlg.h>
#include <timeapi.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "advapi32.lib")
#include <SDL2/SDL_syswm.h>

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl2.h"

// bg modes
#define BG_BLACK     0
#define BG_COLOR     1
#define BG_IMAGE     2
#define BG_SNAPSHOT  3
#define BG_BLUR_SNAP 4

// image fit modes
#define FIT_STRETCH 0
#define FIT_ZOOM    1
#define FIT_TILE    2

#define NUM_BALLS   120
#define NUM_ORBS    10
#define PPM         40.0f
#define PLAYER_SIZE 80

struct Settings {
    int      speed;
    int      fps;
    int      bg_mode;
    float    bg_color[3]; // ImGui uses float[3] for colors
    char     bg_image[512];
    int      bg_fit;
    char     cube_path[512];
    bool     no_ground;
};
static Settings g_settings = { 10, 60, BG_BLACK, {0.12f,0.12f,0.12f}, "", FIT_STRETCH, "", false };

static std::string getCfgPath() {
    char path[MAX_PATH];
    SHGetFolderPathA(NULL,CSIDL_APPDATA,NULL,0,path);
    return std::string(path) + "\\orbit_screensaver.ini";
}
static std::string getAssetDir() {
    char regPath[MAX_PATH]="";
    DWORD sz=MAX_PATH; HKEY key;
    if(RegOpenKeyExA(HKEY_CURRENT_USER,"Software\\Orbit",0,KEY_READ,&key)==ERROR_SUCCESS){
        RegQueryValueExA(key,"InstallDir",0,0,(LPBYTE)regPath,&sz);
        RegCloseKey(key);
    }
    if(regPath[0]) return std::string(regPath);
    char path[MAX_PATH];
    SHGetFolderPathA(NULL,CSIDL_LOCAL_APPDATA,NULL,0,path);
    return std::string(path)+"\\orbit";
}

static void loadCfg() {
    FILE* f=fopen(getCfgPath().c_str(),"r"); if(!f)return;
    char line[640];
    while(fgets(line,sizeof(line),f)){
        int iv; char sv[512]; float fv,fv2,fv3;
        if(sscanf(line,"speed=%d",&iv)==1)           g_settings.speed=iv;
        if(sscanf(line,"fps=%d",&iv)==1)              g_settings.fps=iv;
        if(sscanf(line,"bg_mode=%d",&iv)==1)          g_settings.bg_mode=iv;
        if(sscanf(line,"bg_color=%f,%f,%f",&fv,&fv2,&fv3)==3){g_settings.bg_color[0]=fv;g_settings.bg_color[1]=fv2;g_settings.bg_color[2]=fv3;}
        if(sscanf(line,"bg_fit=%d",&iv)==1)           g_settings.bg_fit=iv;
        if(sscanf(line,"no_ground=%d",&iv)==1)        g_settings.no_ground=(iv!=0);
        if(sscanf(line,"bg_image=%511[^\n]",sv)==1)   strncpy(g_settings.bg_image,sv,511);
        if(sscanf(line,"cube_path=%511[^\n]",sv)==1)  strncpy(g_settings.cube_path,sv,511);
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
    fprintf(f,"bg_image=%s\n",g_settings.bg_image);
    fprintf(f,"cube_path=%s\n",g_settings.cube_path);
    fclose(f);
}

// ── Desktop snapshot ──────────────────────────────────────────────────────
static unsigned char* captureDesktop(int* outW, int* outH) {
    int W=GetSystemMetrics(SM_CXSCREEN), H=GetSystemMetrics(SM_CYSCREEN);
    *outW=W; *outH=H;
    // open the actual user desktop explicitly, not the screensaver desktop
    HDESK hDesk = OpenDesktopA("Default", 0, FALSE, GENERIC_READ);
    HDC screenDC = GetDCEx(GetDesktopWindow(), NULL, DCX_WINDOW);
    if(hDesk) {
        SetThreadDesktop(hDesk);
        ReleaseDC(NULL, screenDC);
        screenDC = GetDC(NULL);
    }
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
static void boxBlur(unsigned char* pixels, int W, int H, int radius) {
    unsigned char* tmp=(unsigned char*)malloc(W*H*4);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int r=0,g=0,b=0,cnt=0;
        for(int k=-radius;k<=radius;k++){int nx=x+k;if(nx<0||nx>=W)continue;int idx=(y*W+nx)*4;r+=pixels[idx];g+=pixels[idx+1];b+=pixels[idx+2];cnt++;}
        int idx=(y*W+x)*4; tmp[idx]=r/cnt;tmp[idx+1]=g/cnt;tmp[idx+2]=b/cnt;tmp[idx+3]=255;
    }
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int r=0,g=0,b=0,cnt=0;
        for(int k=-radius;k<=radius;k++){int ny=y+k;if(ny<0||ny>=H)continue;int idx=(ny*W+x)*4;r+=tmp[idx];g+=tmp[idx+1];b+=tmp[idx+2];cnt++;}
        int idx=(y*W+x)*4; pixels[idx]=r/cnt;pixels[idx+1]=g/cnt;pixels[idx+2]=b/cnt;pixels[idx+3]=255;
    }
    free(tmp);
}

// ── Texture ───────────────────────────────────────────────────────────────
struct Texture { GLuint id; int w,h; bool ok; };

static Texture loadTexture(const char* path) {
    Texture t={0,0,0,false};
    SDL_Surface* surf=IMG_Load(path);
    if(!surf) return t;
    SDL_Surface* conv=SDL_ConvertSurfaceFormat(surf,SDL_PIXELFORMAT_RGBA32,0);
    SDL_FreeSurface(surf); if(!conv)return t;
    glGenTextures(1,&t.id);
    glBindTexture(GL_TEXTURE_2D,t.id);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,conv->w,conv->h,0,GL_RGBA,GL_UNSIGNED_BYTE,conv->pixels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    t.w=conv->w; t.h=conv->h; t.ok=true;
    SDL_FreeSurface(conv); return t;
}
static Texture loadTextureFromPixels(unsigned char* pixels, int w, int h) {
    Texture t={0,0,0,false};
    glGenTextures(1,&t.id);
    glBindTexture(GL_TEXTURE_2D,t.id);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    t.w=w; t.h=h; t.ok=true; return t;
}
static void drawTexturedQuad(GLuint texId,float cx,float cy,float w,float h,float angleDeg) {
    glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D,texId);
    glPushMatrix(); glTranslatef(cx,cy,0); glRotatef(-angleDeg,0,0,1);
    float hw=w/2,hh=h/2;
    glBegin(GL_QUADS);
    glTexCoord2f(0,0);glVertex2f(-hw,-hh); glTexCoord2f(1,0);glVertex2f(hw,-hh);
    glTexCoord2f(1,1);glVertex2f(hw,hh);   glTexCoord2f(0,1);glVertex2f(-hw,hh);
    glEnd(); glPopMatrix();
    glDisable(GL_TEXTURE_2D); glDisable(GL_BLEND);
}
static void drawBgTex(Texture& bg, int W, int H) {
    if(!bg.ok) return;
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,bg.id); glColor4f(1,1,1,1);
    if(g_settings.bg_fit==FIT_ZOOM){
        float sx=(float)W/bg.w,sy=(float)H/bg.h,sc=fmaxf(sx,sy);
        float dw=bg.w*sc,dh=bg.h*sc,ox=(W-dw)/2,oy=(H-dh)/2;
        glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(ox,oy);glTexCoord2f(1,0);glVertex2f(ox+dw,oy);glTexCoord2f(1,1);glVertex2f(ox+dw,oy+dh);glTexCoord2f(0,1);glVertex2f(ox,oy+dh);glEnd();
    } else if(g_settings.bg_fit==FIT_TILE){
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
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

// ── ImGui Settings Window ─────────────────────────────────────────────────
static bool g_preview_clicked = false;

static bool runImGuiSettings() {
    if(SDL_Init(SDL_INIT_VIDEO)<0) return false;
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,0);
    SDL_Window* win=SDL_CreateWindow("Orbit Screensaver - Settings",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        440,480,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    SDL_GLContext ctx=SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io=ImGui::GetIO();
    io.IniFilename=nullptr;
    // load Segoe UI from system
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f);
    if(io.Fonts->Fonts.empty()) io.Fonts->AddFontDefault(); // fallback

    // Dark theme with some colour
    ImGui::StyleColorsDark();
    ImGuiStyle& style=ImGui::GetStyle();
    style.WindowRounding=6; style.FrameRounding=4; style.GrabRounding=4;
    style.Colors[ImGuiCol_Header]        =ImVec4(0.26f,0.59f,0.98f,0.4f);
    style.Colors[ImGuiCol_HeaderHovered] =ImVec4(0.26f,0.59f,0.98f,0.6f);
    style.Colors[ImGuiCol_Button]        =ImVec4(0.26f,0.59f,0.98f,0.5f);
    style.Colors[ImGuiCol_ButtonHovered] =ImVec4(0.26f,0.59f,0.98f,0.8f);

    ImGui_ImplSDL2_InitForOpenGL(win,ctx);
    ImGui_ImplOpenGL2_Init();

    const char* bgNames[]={"Black","Custom Color","Image","Transparent (snapshot)","Blur (snapshot)"};
    const char* fitNames[]={"Stretch","Zoom","Tile"};
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
        ImGui::Begin("##main",nullptr,
            ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
            ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar);

        ImGui::TextColored(ImVec4(0.4f,0.8f,1.0f,1.0f),"ORBIT SCREENSAVER");
        ImGui::Separator();
        ImGui::Spacing();

        // speed
        ImGui::SliderInt("Speed",&g_settings.speed,1,20);

        // fps
        ImGui::SetNextItemWidth(80);
        ImGui::InputInt("FPS",&g_settings.fps,0);
        if(g_settings.fps<1)g_settings.fps=1;
        if(g_settings.fps>500)g_settings.fps=500;
        ImGui::SameLine();
        int fpsPresets[]={30,60,120,144,240,500};
        for(int fp:fpsPresets){
            char lbl[8]; sprintf(lbl,"%d",fp);
            if(ImGui::SmallButton(lbl)) g_settings.fps=fp;
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::Spacing();

        // cube path
        ImGui::Text("Cube PNG");
        ImGui::SetNextItemWidth(280);
        ImGui::InputText("##cube",g_settings.cube_path,sizeof(g_settings.cube_path));
        ImGui::SameLine();
        if(ImGui::Button("Browse##cube")){
            OPENFILENAMEA ofn={};char buf[512]="";
            ofn.lStructSize=sizeof(ofn);
            ofn.lpstrFilter="PNG\0*.png\0All\0*.*\0";
            ofn.lpstrFile=buf;ofn.nMaxFile=sizeof(buf);ofn.Flags=OFN_FILEMUSTEXIST;
            if(GetOpenFileNameA(&ofn)) strncpy(g_settings.cube_path,buf,511);
        }
        ImGui::Spacing();

        // background
        ImGui::Text("Background");
        ImGui::SetNextItemWidth(200);
        ImGui::Combo("##bg",&g_settings.bg_mode,bgNames,5);

        if(g_settings.bg_mode==BG_COLOR){
            ImGui::ColorEdit3("Color",&g_settings.bg_color[0]);
        }
        if(g_settings.bg_mode==BG_IMAGE){
            ImGui::SetNextItemWidth(280);
            ImGui::InputText("##img",g_settings.bg_image,sizeof(g_settings.bg_image));
            ImGui::SameLine();
            if(ImGui::Button("Browse##img")){
                OPENFILENAMEA ofn={};char buf[512]="";
                ofn.lStructSize=sizeof(ofn);
                ofn.lpstrFilter="Images\0*.png;*.jpg;*.bmp\0All\0*.*\0";
                ofn.lpstrFile=buf;ofn.nMaxFile=sizeof(buf);ofn.Flags=OFN_FILEMUSTEXIST;
                if(GetOpenFileNameA(&ofn)) strncpy(g_settings.bg_image,buf,511);
            }
            ImGui::Text("Fit:"); ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            ImGui::Combo("##fit",&g_settings.bg_fit,fitNames,3);
        }
        ImGui::Spacing();

        // no ground
        ImGui::Checkbox("No ground (infinite fall)",&g_settings.no_ground);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // buttons
        if(ImGui::Button("Save",ImVec2(100,30))){
            saveCfg();
            ImGui::OpenPopup("Saved");
        }
        ImGui::SameLine();
        if(ImGui::Button("Preview",ImVec2(100,30))){
            saveCfg();
            g_preview_clicked=true;
            running=false;
        }
        if(ImGui::BeginPopupModal("Saved",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::Text("Settings saved!");
            if(ImGui::Button("OK")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::Text("by "); ImGui::SameLine();
        if(ImGui::SmallButton("MalikHw47")) ShellExecuteA(0,"open","https://malikhw.github.io",0,0,SW_SHOW);
        ImGui::SameLine(); ImGui::Text("-"); ImGui::SameLine();
        if(ImGui::SmallButton("youtube"))   ShellExecuteA(0,"open","https://youtube.com/@MalikHw47",0,0,SW_SHOW);
        ImGui::SameLine(); ImGui::Text("-"); ImGui::SameLine();
        if(ImGui::SmallButton("github"))    ShellExecuteA(0,"open","https://github.com/MalikHw",0,0,SW_SHOW);

        ImGui::End();
        ImGui::Render();
        glViewport(0,0,W,H);
        glClearColor(0.1f,0.1f,0.1f,1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(win);
    }

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return g_preview_clicked;
}

// ── Screensaver loop ──────────────────────────────────────────────────────
static void runScreensaver(bool isPreview, void* previewHandle) {
    HWND parentHwnd=(HWND)previewHandle;
    if(isPreview && parentHwnd){
        char e[128];
        sprintf(e,"SDL_VIDEODRIVER=windib"); putenv(e);
        sprintf(e,"SDL_WINDOWID=%llu",(unsigned long long)(uintptr_t)parentHwnd); putenv(e);
    }
    // capture BEFORE SDL covers the screen
    bool needSnap=!isPreview&&(g_settings.bg_mode==BG_SNAPSHOT||g_settings.bg_mode==BG_BLUR_SNAP);
    unsigned char* snapPixels=nullptr; int snapW=0,snapH=0;
    if(needSnap){
        snapPixels=captureDesktop(&snapW,&snapH);
        if(g_settings.bg_mode==BG_BLUR_SNAP) boxBlur(snapPixels,snapW,snapH,12);
    }

    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0){if(snapPixels)free(snapPixels);return;}
    IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);

    int W,H;
    if(isPreview){ RECT rc;GetClientRect(parentHwnd,&rc);W=rc.right-rc.left;if(W<=0)W=152;H=rc.bottom-rc.top;if(H<=0)H=112; }
    else { SDL_DisplayMode dm;SDL_GetCurrentDisplayMode(0,&dm);W=dm.w;H=dm.h; }

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8); SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8); SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1); SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,0);

    SDL_Window* win=isPreview
        ? SDL_CreateWindow("orbit",0,0,W,H,SDL_WINDOW_OPENGL)
        : SDL_CreateWindow("orbit",0,0,W,H,SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN_DESKTOP|SDL_WINDOW_BORDERLESS);
    if(!isPreview) SDL_ShowCursor(SDL_DISABLE);
    if(!win){SDL_Quit();return;}

    SDL_GLContext ctx=SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,W,H,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable(GL_DEPTH_TEST);

    Texture snapTex={0,0,0,false};
    if(needSnap && snapPixels){
        snapTex=loadTextureFromPixels(snapPixels,snapW,snapH);
        free(snapPixels); snapPixels=nullptr;
    }

    std::string assetDir=getAssetDir();
    Texture orbTex[NUM_ORBS];
    for(int i=0;i<NUM_ORBS;i++){char p[600];snprintf(p,sizeof(p),"%s/orb%d.png",assetDir.c_str(),i+1);orbTex[i]=loadTexture(p);}
    Texture cubeTex={0,0,0,false};
    {const char* cs=g_settings.cube_path[0]?g_settings.cube_path:nullptr;if(!cs){char p[600];snprintf(p,sizeof(p),"%s/cube.png",assetDir.c_str());cubeTex=loadTexture(p);}else cubeTex=loadTexture(cs);}
    Texture bgTex={0,0,0,false};
    if(g_settings.bg_mode==BG_IMAGE&&g_settings.bg_image[0]) bgTex=loadTexture(g_settings.bg_image);

    srand((unsigned)time(nullptr));
    bool running=true;

    while(running){
        srand((unsigned)rand());
        int fps=g_settings.fps; if(fps<1)fps=1; if(fps>500)fps=500;
        float speedMult=g_settings.speed/10.0f;
        b2Vec2 gravity(0.0f,9.8f*speedMult*3.0f);
        b2World world(gravity);
        int dropTime=(int)(20.0f/speedMult); if(dropTime<1)dropTime=1;

        auto makeWall=[&](float x1,float y1,float x2,float y2){
            b2BodyDef bd;bd.type=b2_staticBody;b2Body* b=world.CreateBody(&bd);
            b2EdgeShape es;es.SetTwoSided(b2Vec2(x1/PPM,y1/PPM),b2Vec2(x2/PPM,y2/PPM));
            b2FixtureDef fd;fd.shape=&es;fd.restitution=0.5f;fd.friction=0.7f;
            b->CreateFixture(&fd);return b;
        };
        makeWall(0,0,0,H); makeWall(W,0,W,H);
        b2Body* wallBottom=nullptr;
        if(!g_settings.no_ground) wallBottom=makeWall(0,H,W,H);

        std::vector<Ball> balls;
        int globalTime=0; bool fillingDone=false,draining=false;
        SDL_Point lastMouse; SDL_GetMouseState(&lastMouse.x,&lastMouse.y);
        int grace=60;
        Uint32 lastTick=SDL_GetTicks(); float physAccum=0; const float physStep=1.0f/fps;
        bool simRunning=true;

        while(simRunning&&running){
            globalTime++;
            SDL_Event ev;
            while(SDL_PollEvent(&ev)){
                if(ev.type==SDL_QUIT){running=false;simRunning=false;}
                if(globalTime>grace&&!isPreview){
                    if(ev.type==SDL_KEYDOWN||ev.type==SDL_MOUSEBUTTONDOWN){running=false;simRunning=false;}
                    if(ev.type==SDL_MOUSEMOTION&&(ev.motion.x!=lastMouse.x||ev.motion.y!=lastMouse.y)){running=false;simRunning=false;}
                }
            }
            if(isPreview&&parentHwnd&&!IsWindow(parentHwnd)){running=false;simRunning=false;}

            for(int i=0;i<NUM_BALLS;i++){
                if(globalTime==dropTime*i){
                    float radius=40+rand()%20;
                    b2BodyDef bd;bd.type=b2_dynamicBody;
                    bd.position.Set(((float)W*0.8f/NUM_BALLS*(1+rand()%(NUM_BALLS*2)))/PPM,-250.0f/PPM);
                    b2Body* body=world.CreateBody(&bd);
                    b2CircleShape cs;cs.m_radius=radius/PPM;
                    b2FixtureDef fd;fd.shape=&cs;fd.density=1.0f;fd.restitution=0.5f;fd.friction=1.0f;
                    body->CreateFixture(&fd);
                    body->ApplyLinearImpulse(b2Vec2((10-rand()%21)*0.05f,0),body->GetWorldCenter(),true);
                    Ball ball;ball.body=body;ball.radius=radius;ball.orbIdx=rand()%NUM_ORBS;ball.isPlayer=false;
                    balls.push_back(ball);
                }
            }
            if(globalTime==50*dropTime){
                b2BodyDef bd;bd.type=b2_dynamicBody;bd.position.Set((float)W*0.5f/PPM,-400.0f/PPM);
                b2Body* body=world.CreateBody(&bd);
                b2PolygonShape ps;ps.SetAsBox(PLAYER_SIZE*0.5f/PPM,PLAYER_SIZE*0.5f/PPM);
                b2FixtureDef fd;fd.shape=&ps;fd.density=1.0f;fd.restitution=0.5f;fd.friction=0.7f;
                body->CreateFixture(&fd);
                Ball ball;ball.body=body;ball.radius=PLAYER_SIZE*0.5f;ball.orbIdx=0;ball.isPlayer=true;
                balls.push_back(ball);
            }
            if(!g_settings.no_ground){
                if(!fillingDone&&globalTime>NUM_BALLS*dropTime+200){fillingDone=true;draining=true;if(wallBottom)world.DestroyBody(wallBottom);}
                if(draining){bool allOff=true;for(auto& b:balls)if(b.body->GetPosition().y*PPM<H+300){allOff=false;break;}if(allOff)simRunning=false;}
            }
            if(g_settings.no_ground&&globalTime>NUM_BALLS*dropTime+500) simRunning=false;

            Uint32 now=SDL_GetTicks();
            physAccum+=(now-lastTick)/1000.0f; lastTick=now;
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
                    float s=PLAYER_SIZE;
                    if(cubeTex.ok) drawTexturedQuad(cubeTex.id,px,py,s,s,ang);
                    else{glColor3f(0.78f,0.39f,0.39f);glPushMatrix();glTranslatef(px,py,0);glRotatef(-ang,0,0,1);float h2=s/2;glBegin(GL_QUADS);glVertex2f(-h2,-h2);glVertex2f(h2,-h2);glVertex2f(h2,h2);glVertex2f(-h2,h2);glEnd();glPopMatrix();glColor3f(1,1,1);}
                } else {
                    float d=b.radius*2;
                    if(orbTex[b.orbIdx].ok) drawTexturedQuad(orbTex[b.orbIdx].id,px,py,d,d,ang);
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
    SDL_GL_DeleteContext(ctx); SDL_DestroyWindow(win);
    IMG_Quit(); SDL_Quit();
}

// ── Entry point ───────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int){
    timeBeginPeriod(1);
    loadCfg();

    int argc; LPWSTR* wargv=CommandLineToArgvW(GetCommandLineW(),&argc);
    bool doConfig=false,doPreview=false;
    HWND previewHwnd=nullptr;
    for(int i=1;i<argc;i++){
        char a[64]; WideCharToMultiByte(CP_ACP,0,wargv[i],-1,a,sizeof(a),0,0);
        for(char* p=a;*p;p++) *p=tolower(*p);
        if(!strncmp(a,"/c",2)||!strncmp(a,"-c",2)) doConfig=true;
        else if(!strcmp(a,"/p")||!strcmp(a,"-p")){
            doPreview=true;
            if(i+1<argc){char b[32];WideCharToMultiByte(CP_ACP,0,wargv[i+1],-1,b,sizeof(b),0,0);previewHwnd=(HWND)(uintptr_t)atoll(b);}
        }
    }
    LocalFree(wargv);

    if(doConfig){ bool prev=runImGuiSettings(); if(prev)runScreensaver(false,nullptr); }
    else if(doPreview) runScreensaver(true,(void*)previewHwnd);
    else runScreensaver(false,nullptr);

    timeEndPeriod(1);
    return 0;
}
