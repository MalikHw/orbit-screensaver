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
#include <commctrl.h>
#include <timeapi.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")
#include <SDL2/SDL_syswm.h>

// bg modes
#define BG_BLACK  0
#define BG_COLOR  1
#define BG_IMAGE  2

// image fit modes
#define FIT_STRETCH 0
#define FIT_ZOOM    1
#define FIT_TILE    2

#define NUM_BALLS  120
#define NUM_ORBS   10
#define PPM        40.0f
#define PLAYER_SIZE 80

struct Settings {
    int   speed;      // 1-20
    int   fps;        // e.g. 60
    int   bg_mode;
    COLORREF bg_color;
    char  bg_image[512];
    int   bg_fit;
    char  cube_path[512];
};
static Settings g_settings = { 10, 60, BG_BLACK, RGB(30,30,30), "", FIT_STRETCH, "" };

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
        int iv; char sv[512]; unsigned int uv;
        if(sscanf(line,"speed=%d",&iv)==1)      g_settings.speed=iv;
        if(sscanf(line,"fps=%d",&iv)==1)         g_settings.fps=iv;
        if(sscanf(line,"bg_mode=%d",&iv)==1)     g_settings.bg_mode=iv;
        if(sscanf(line,"bg_color=%u",&uv)==1)    g_settings.bg_color=(COLORREF)uv;
        if(sscanf(line,"bg_fit=%d",&iv)==1)      g_settings.bg_fit=iv;
        if(sscanf(line,"bg_image=%511[^\n]",sv)==1) strncpy(g_settings.bg_image,sv,511);
        if(sscanf(line,"cube_path=%511[^\n]",sv)==1) strncpy(g_settings.cube_path,sv,511);
    }
    fclose(f);
}
static void saveCfg() {
    FILE* f=fopen(getCfgPath().c_str(),"w"); if(!f)return;
    fprintf(f,"speed=%d\n",g_settings.speed);
    fprintf(f,"fps=%d\n",g_settings.fps);
    fprintf(f,"bg_mode=%d\n",g_settings.bg_mode);
    fprintf(f,"bg_color=%u\n",(unsigned)g_settings.bg_color);
    fprintf(f,"bg_fit=%d\n",g_settings.bg_fit);
    fprintf(f,"bg_image=%s\n",g_settings.bg_image);
    fprintf(f,"cube_path=%s\n",g_settings.cube_path);
    fclose(f);
}

// ── Win32 settings dialog ─────────────────────────────────────────────────
#define IDC_SPEED_SLIDER  101
#define IDC_SPEED_LABEL   102
#define IDC_CUBE_EDIT     103
#define IDC_CUBE_BROWSE   104
#define IDC_BG_BLACK      105
#define IDC_BG_COLOR      106
#define IDC_BG_IMAGE      107
#define IDC_COLOR_PICK    108
#define IDC_IMG_EDIT      109
#define IDC_IMG_BROWSE    110
#define IDC_FIT_STRETCH   111
#define IDC_FIT_ZOOM      112
#define IDC_FIT_TILE      113
#define IDC_SAVE          114
#define IDC_PREVIEW       115
#define IDC_FPS_EDIT      116
#define IDC_FPS_30        117
#define IDC_FPS_60        118
#define IDC_FPS_120       119
#define IDC_FPS_144       120
#define IDC_FPS_240       121
#define IDC_FPS_500       122
#define IDC_COLOR_PREVIEW 123

static bool g_preview_clicked = false;
static COLORREF g_customColors[16] = {};

static void updateColorPreview(HWND hwnd) {
    InvalidateRect(GetDlgItem(hwnd,IDC_COLOR_PREVIEW),NULL,TRUE);
}

static LRESULT CALLBACK SettingsDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch(msg) {
    case WM_CREATE: {
        HFONT font = CreateFontA(14,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Consolas");

        auto mkCtrl = [&](const char* cls, const char* t, DWORD style, int id, int x, int y, int w, int h) {
            HWND c = CreateWindowA(cls,t,WS_CHILD|WS_VISIBLE|style,x,y,w,h,hwnd,(HMENU)(intptr_t)id,0,0);
            SendMessage(c,WM_SETFONT,(WPARAM)font,TRUE); return c;
        };
        auto mkLabel  = [&](const char* t, int x,int y,int w,int h){ return mkCtrl("STATIC",t,0,0,x,y,w,h); };
        auto mkBtn    = [&](const char* t, int id,int x,int y,int w,int h){ return mkCtrl("BUTTON",t,BS_PUSHBUTTON,id,x,y,w,h); };
        auto mkRadio  = [&](const char* t, int id,int x,int y,int w,int h,bool first){
            return mkCtrl("BUTTON",t,BS_AUTORADIOBUTTON|(first?WS_GROUP:0),id,x,y,w,h);
        };
        auto mkEdit   = [&](const char* t, int id,int x,int y,int w,int h){
            return mkCtrl("EDIT",t,WS_BORDER|ES_AUTOHSCROLL,id,x,y,w,h);
        };
        auto mkLink   = [&](const char* t, int id,int x,int y,int w,int h){
            return mkCtrl("BUTTON",t,BS_OWNERDRAW,id,x,y,w,h);
        };

        int y=10;
        mkLabel("ORBIT SCREENSAVER",10,y,280,20); y+=28;

        // speed
        mkLabel("Speed (1-20)",10,y+4,80,18);
        HWND sl=CreateWindowA(TRACKBAR_CLASSA,"",WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS,
            100,y,180,24,hwnd,(HMENU)IDC_SPEED_SLIDER,0,0);
        SendMessage(sl,TBM_SETRANGE,TRUE,MAKELPARAM(1,20));
        SendMessage(sl,TBM_SETPOS,TRUE,g_settings.speed);
        SendMessage(sl,WM_SETFONT,(WPARAM)font,TRUE);
        char spdbuf[8]; sprintf(spdbuf,"%d",g_settings.speed);
        mkCtrl("STATIC",spdbuf,0,IDC_SPEED_LABEL,284,y+4,30,18); y+=30;

        // fps
        mkLabel("FPS",10,y+3,40,18);
        char fpsbuf[8]; sprintf(fpsbuf,"%d",g_settings.fps);
        mkEdit(fpsbuf,IDC_FPS_EDIT,55,y,50,22);
        // preset buttons
        mkBtn("30", IDC_FPS_30, 112,y,32,22);
        mkBtn("60", IDC_FPS_60, 148,y,32,22);
        mkBtn("120",IDC_FPS_120,184,y,36,22);
        mkBtn("144",IDC_FPS_144,224,y,36,22);
        mkBtn("240",IDC_FPS_240,264,y,36,22);
        mkBtn("500",IDC_FPS_500,304,y,36,22);
        y+=30;

        // cube path
        mkLabel("Cube PNG",10,y+3,60,18);
        mkEdit(g_settings.cube_path,IDC_CUBE_EDIT,75,y,220,22);
        mkBtn("...",IDC_CUBE_BROWSE,300,y,30,22); y+=30;

        // background
        mkLabel("Background",10,y,80,18); y+=22;
        mkRadio("Black",    IDC_BG_BLACK,20,y,60,18,true);
        mkRadio("Color",    IDC_BG_COLOR,90,y,55,18,false);
        // color preview box (owner-draw static)
        CreateWindowA("STATIC","",WS_CHILD|WS_VISIBLE|SS_OWNERDRAW,
            150,y,24,18,hwnd,(HMENU)IDC_COLOR_PREVIEW,0,0);
        mkBtn("Pick",IDC_COLOR_PICK,178,y,40,18);
        mkRadio("Image",    IDC_BG_IMAGE,228,y,60,18,false);
        y+=24;

        // image path (visible only when image selected, but always present)
        mkEdit(g_settings.bg_image,IDC_IMG_EDIT,20,y,260,22);
        mkBtn("...",IDC_IMG_BROWSE,284,y,30,22); y+=26;

        // fit mode
        mkLabel("Fit:",20,y,25,18);
        mkRadio("Stretch",IDC_FIT_STRETCH,50, y,65,18,true);
        mkRadio("Zoom",   IDC_FIT_ZOOM,  120,y,50,18,false);
        mkRadio("Tile",   IDC_FIT_TILE,  175,y,45,18,false);
        y+=28;

        // check radios
        int bgIds[]={IDC_BG_BLACK,IDC_BG_COLOR,IDC_BG_IMAGE};
        CheckDlgButton(hwnd,bgIds[g_settings.bg_mode],BST_CHECKED);
        int fitIds[]={IDC_FIT_STRETCH,IDC_FIT_ZOOM,IDC_FIT_TILE};
        CheckDlgButton(hwnd,fitIds[g_settings.bg_fit],BST_CHECKED);

        mkBtn("Save",   IDC_SAVE,   60,y,80,26);
        mkBtn("Preview",IDC_PREVIEW,160,y,80,26); y+=36;

        // credits
        mkLabel("by ",10,y,20,18);
        mkLink("MalikHw47",201,30,y,70,18);
        mkLabel("-",102,y,8,18);
        mkLink("youtube",202,112,y,55,18);
        mkLabel("-",169,y,8,18);
        mkLink("github",203,179,y,50,18);

        // resize window to fit content
        SetWindowPos(hwnd,0,0,0,360,y+60,SWP_NOMOVE|SWP_NOZORDER);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HWND ctrl=(HWND)lp;
        if(GetDlgCtrlID(ctrl)==IDC_COLOR_PREVIEW){
            HDC hdc=(HDC)wp;
            SetBkColor(hdc,g_settings.bg_color);
            static HBRUSH br=NULL;
            if(br) DeleteObject(br);
            br=CreateSolidBrush(g_settings.bg_color);
            return (LRESULT)br;
        }
        return DefWindowProcA(hwnd,msg,wp,lp);
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* di=(DRAWITEMSTRUCT*)lp;
        // link buttons
        int id=di->CtlID;
        if(id==201||id==202||id==203){
            SetBkMode(di->hDC,TRANSPARENT);
            SetTextColor(di->hDC,RGB(85,136,255));
            char txt[64]; GetWindowTextA(di->hwndItem,txt,sizeof(txt));
            DrawTextA(di->hDC,txt,-1,&di->rcItem,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
            return TRUE;
        }
        return FALSE;
    }
    case WM_SETCURSOR: {
        int id=GetDlgCtrlID((HWND)wp);
        if(id==201||id==202||id==203){SetCursor(LoadCursor(0,IDC_HAND));return TRUE;}
        return DefWindowProcA(hwnd,msg,wp,lp);
    }
    case WM_HSCROLL: {
        if((HWND)lp==GetDlgItem(hwnd,IDC_SPEED_SLIDER)){
            int v=(int)SendDlgItemMessage(hwnd,IDC_SPEED_SLIDER,TBM_GETPOS,0,0);
            char s[8]; sprintf(s,"%d",v);
            SetDlgItemTextA(hwnd,IDC_SPEED_LABEL,s);
        }
        return 0;
    }
    case WM_COMMAND: {
        int id=LOWORD(wp);
        // fps presets
        int fpsPresets[]={30,60,120,144,240,500};
        int fpsIds[]={IDC_FPS_30,IDC_FPS_60,IDC_FPS_120,IDC_FPS_144,IDC_FPS_240,IDC_FPS_500};
        for(int i=0;i<6;i++) if(id==fpsIds[i]){char b[8];sprintf(b,"%d",fpsPresets[i]);SetDlgItemTextA(hwnd,IDC_FPS_EDIT,b);}

        if(id==IDC_CUBE_BROWSE){
            OPENFILENAMEA ofn={};char buf[512]="";
            ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=hwnd;
            ofn.lpstrFilter="PNG\0*.png\0All\0*.*\0";
            ofn.lpstrFile=buf;ofn.nMaxFile=sizeof(buf);ofn.Flags=OFN_FILEMUSTEXIST;
            if(GetOpenFileNameA(&ofn)) SetDlgItemTextA(hwnd,IDC_CUBE_EDIT,buf);
        }
        if(id==IDC_IMG_BROWSE){
            OPENFILENAMEA ofn={};char buf[512]="";
            ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=hwnd;
            ofn.lpstrFilter="Images\0*.png;*.jpg;*.bmp\0All\0*.*\0";
            ofn.lpstrFile=buf;ofn.nMaxFile=sizeof(buf);ofn.Flags=OFN_FILEMUSTEXIST;
            if(GetOpenFileNameA(&ofn)) SetDlgItemTextA(hwnd,IDC_IMG_EDIT,buf);
        }
        if(id==IDC_COLOR_PICK){
            CHOOSECOLORA cc={};
            cc.lStructSize=sizeof(cc);cc.hwndOwner=hwnd;
            cc.lpCustColors=g_customColors;
            cc.rgbResult=g_settings.bg_color;
            cc.Flags=CC_FULLOPEN|CC_RGBINIT;
            if(ChooseColorA(&cc)){
                g_settings.bg_color=cc.rgbResult;
                updateColorPreview(hwnd);
            }
        }
        if(id==201) ShellExecuteA(hwnd,"open","https://malikhw.github.io",0,0,SW_SHOW);
        if(id==202) ShellExecuteA(hwnd,"open","https://youtube.com/@MalikHw47",0,0,SW_SHOW);
        if(id==203) ShellExecuteA(hwnd,"open","https://github.com/MalikHw",0,0,SW_SHOW);

        if(id==IDC_SAVE||id==IDC_PREVIEW){
            g_settings.speed=(int)SendDlgItemMessage(hwnd,IDC_SPEED_SLIDER,TBM_GETPOS,0,0);
            char fpsbuf[16]; GetDlgItemTextA(hwnd,IDC_FPS_EDIT,fpsbuf,sizeof(fpsbuf));
            int fpsval=atoi(fpsbuf); if(fpsval<1)fpsval=1; if(fpsval>500)fpsval=500;
            g_settings.fps=fpsval;
            char buf[512];
            GetDlgItemTextA(hwnd,IDC_CUBE_EDIT,buf,sizeof(buf)); strncpy(g_settings.cube_path,buf,511);
            GetDlgItemTextA(hwnd,IDC_IMG_EDIT,buf,sizeof(buf));  strncpy(g_settings.bg_image,buf,511);
            int bgIds[]={IDC_BG_BLACK,IDC_BG_COLOR,IDC_BG_IMAGE};
            for(int i=0;i<3;i++) if(IsDlgButtonChecked(hwnd,bgIds[i])) g_settings.bg_mode=i;
            int fitIds[]={IDC_FIT_STRETCH,IDC_FIT_ZOOM,IDC_FIT_TILE};
            for(int i=0;i<3;i++) if(IsDlgButtonChecked(hwnd,fitIds[i])) g_settings.bg_fit=i;
            saveCfg();
            if(id==IDC_PREVIEW){g_preview_clicked=true;DestroyWindow(hwnd);}
            else MessageBoxA(hwnd,"Settings saved!","Orbit",MB_OK|MB_ICONINFORMATION);
        }
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd,msg,wp,lp);
}

static bool runWin32Settings() {
    g_preview_clicked=false;
    WNDCLASSA wc={};
    wc.lpfnWndProc=SettingsDlgProc;
    wc.lpszClassName="OrbitSettings";
    wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    wc.hCursor=LoadCursor(0,IDC_ARROW);
    RegisterClassA(&wc);
    HWND hwnd=CreateWindowA("OrbitSettings","Orbit Screensaver - Settings",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
        CW_USEDEFAULT,CW_USEDEFAULT,360,400,0,0,0,0);
    ShowWindow(hwnd,SW_SHOW); UpdateWindow(hwnd);
    MSG msg;
    while(GetMessage(&msg,0,0,0)){TranslateMessage(&msg);DispatchMessage(&msg);}
    return g_preview_clicked;
}

// ── Texture ───────────────────────────────────────────────────────────────
struct Texture { GLuint id; int w,h; bool ok; };

static Texture loadTexture(const char* path) {
    Texture t={0,0,0,false};
    SDL_Surface* surf=IMG_Load(path);
    if(!surf){fprintf(stderr,"cant load %s: %s\n",path,IMG_GetError());return t;}
    SDL_Surface* conv=SDL_ConvertSurfaceFormat(surf,SDL_PIXELFORMAT_RGBA32,0);
    SDL_FreeSurface(surf); if(!conv)return t;
    glGenTextures(1,&t.id);
    glBindTexture(GL_TEXTURE_2D,t.id);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,conv->w,conv->h,0,GL_RGBA,GL_UNSIGNED_BYTE,conv->pixels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    t.w=conv->w; t.h=conv->h; t.ok=true;
    SDL_FreeSurface(conv);
    return t;
}

static void drawTexturedQuad(GLuint texId,float cx,float cy,float w,float h,float angleDeg) {
    glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D,texId);
    glPushMatrix(); glTranslatef(cx,cy,0); glRotatef(-angleDeg,0,0,1);
    float hw=w/2,hh=h/2;
    glBegin(GL_QUADS);
    glTexCoord2f(0,0);glVertex2f(-hw,-hh);
    glTexCoord2f(1,0);glVertex2f( hw,-hh);
    glTexCoord2f(1,1);glVertex2f( hw, hh);
    glTexCoord2f(0,1);glVertex2f(-hw, hh);
    glEnd();
    glPopMatrix();
    glDisable(GL_TEXTURE_2D); glDisable(GL_BLEND);
}

static void drawBgImage(Texture& bg, int W, int H) {
    if(!bg.ok) return;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,bg.id);
    glColor4f(1,1,1,1);

    if(g_settings.bg_fit==FIT_STRETCH) {
        glBegin(GL_QUADS);
        glTexCoord2f(0,0);glVertex2f(0,0);
        glTexCoord2f(1,0);glVertex2f(W,0);
        glTexCoord2f(1,1);glVertex2f(W,H);
        glTexCoord2f(0,1);glVertex2f(0,H);
        glEnd();
    } else if(g_settings.bg_fit==FIT_ZOOM) {
        // scale to fill, crop edges
        float scaleX=(float)W/bg.w, scaleY=(float)H/bg.h;
        float scale=fmaxf(scaleX,scaleY);
        float dw=bg.w*scale, dh=bg.h*scale;
        float ox=(W-dw)/2, oy=(H-dh)/2;
        glBegin(GL_QUADS);
        glTexCoord2f(0,0);glVertex2f(ox,oy);
        glTexCoord2f(1,0);glVertex2f(ox+dw,oy);
        glTexCoord2f(1,1);glVertex2f(ox+dw,oy+dh);
        glTexCoord2f(0,1);glVertex2f(ox,oy+dh);
        glEnd();
    } else { // tile
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        float tx=(float)W/bg.w, ty=(float)H/bg.h;
        glBegin(GL_QUADS);
        glTexCoord2f(0,0); glVertex2f(0,0);
        glTexCoord2f(tx,0);glVertex2f(W,0);
        glTexCoord2f(tx,ty);glVertex2f(W,H);
        glTexCoord2f(0,ty);glVertex2f(0,H);
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
}

static void drawCircleFallback(float cx,float cy,float r) {
    glColor3f(0.39f,0.39f,0.78f);
    glBegin(GL_TRIANGLE_FAN); glVertex2f(cx,cy);
    for(int i=0;i<=32;i++){float a=i*2*(float)M_PI/32;glVertex2f(cx+cosf(a)*r,cy+sinf(a)*r);}
    glEnd(); glColor3f(1,1,1);
}

struct Ball { b2Body* body; float radius; int orbIdx; bool isPlayer; };

// ── Screensaver loop ──────────────────────────────────────────────────────
static void runScreensaver(bool isPreview, void* previewHandle) {
    HWND parentHwnd=(HWND)previewHandle;
    if(isPreview && parentHwnd){
        char e[128];
        sprintf(e,"SDL_VIDEODRIVER=windib"); putenv(e);
        sprintf(e,"SDL_WINDOWID=%llu",(unsigned long long)(uintptr_t)parentHwnd); putenv(e);
    }
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0){fprintf(stderr,"sdl init failed\n");return;}
    IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);

    int W,H;
    SDL_Window* win;

    if(isPreview){
        RECT rc; GetClientRect(parentHwnd,&rc);
        W=rc.right-rc.left; if(W<=0)W=152;
        H=rc.bottom-rc.top; if(H<=0)H=112;
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,0);
        win=SDL_CreateWindow("orbit",0,0,W,H,SDL_WINDOW_OPENGL);
    } else {
        SDL_DisplayMode dm; SDL_GetCurrentDisplayMode(0,&dm);
        W=dm.w; H=dm.h;
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,0);
        win=SDL_CreateWindow("orbit",0,0,W,H,SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN_DESKTOP|SDL_WINDOW_BORDERLESS);
        SDL_ShowCursor(SDL_DISABLE);
    }

    if(!win){fprintf(stderr,"window failed\n");SDL_Quit();return;}
    SDL_GLContext ctx=SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);

    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0,W,H,0,-1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST);

    std::string assetDir=getAssetDir();
    Texture orbTex[NUM_ORBS];
    for(int i=0;i<NUM_ORBS;i++){
        char p[600]; snprintf(p,sizeof(p),"%s/orb%d.png",assetDir.c_str(),i+1);
        orbTex[i]=loadTexture(p);
    }
    Texture cubeTex={0,0,0,false};
    {
        const char* src=g_settings.cube_path[0]?g_settings.cube_path:nullptr;
        if(!src){char p[600];snprintf(p,sizeof(p),"%s/cube.png",assetDir.c_str());cubeTex=loadTexture(p);}
        else cubeTex=loadTexture(src);
    }
    Texture bgTex={0,0,0,false};
    if(g_settings.bg_mode==BG_IMAGE && g_settings.bg_image[0])
        bgTex=loadTexture(g_settings.bg_image);

    srand((unsigned)time(nullptr));
    bool running=true, firstSim=true;

    while(running){
        unsigned seed=(unsigned)rand(); srand(seed);

        int fps=g_settings.fps; if(fps<1)fps=1; if(fps>500)fps=500;
        float speedMult=g_settings.speed/10.0f;

        // speedhack: scale gravity AND drop timing
        b2Vec2 gravity(0.0f, 9.8f*speedMult*3.0f);
        b2World world(gravity);

        // drop_time scales inversely with speed: speed=20 -> dropTime=1, speed=1 -> dropTime=20
        int dropTime=(int)(20.0f/speedMult); if(dropTime<1)dropTime=1;

        auto makeWall=[&](float x1,float y1,float x2,float y2){
            b2BodyDef bd; bd.type=b2_staticBody;
            b2Body* b=world.CreateBody(&bd);
            b2EdgeShape es; es.SetTwoSided(b2Vec2(x1/PPM,y1/PPM),b2Vec2(x2/PPM,y2/PPM));
            b2FixtureDef fd; fd.shape=&es; fd.restitution=0.5f; fd.friction=0.7f;
            b->CreateFixture(&fd); return b;
        };
        b2Body* wallBottom=makeWall(0,H,W,H);
        b2Body* wallLeft  =makeWall(0,0,0,H);
        b2Body* wallRight =makeWall(W,0,W,H);
        (void)wallLeft;(void)wallRight;

        std::vector<Ball> balls;
        int globalTime=0;
        bool fillingDone=false,draining=false;

        SDL_Point lastMouse; SDL_GetMouseState(&lastMouse.x,&lastMouse.y);
        int grace=60;

        Uint32 lastTick=SDL_GetTicks();
        float physAccum=0.0f;
        const float physStep=1.0f/fps;

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
            // die if preview parent closes
            if(isPreview&&parentHwnd&&!IsWindow(parentHwnd)){running=false;simRunning=false;}

            // spawn balls
            for(int i=0;i<NUM_BALLS;i++){
                if(globalTime==dropTime*i){
                    float radius=40+rand()%20;
                    b2BodyDef bd; bd.type=b2_dynamicBody;
                    bd.position.Set(((float)W*0.8f/NUM_BALLS*(1+rand()%(NUM_BALLS*2)))/PPM,-250.0f/PPM);
                    b2Body* body=world.CreateBody(&bd);
                    b2CircleShape cs; cs.m_radius=radius/PPM;
                    b2FixtureDef fd; fd.shape=&cs; fd.density=1.0f; fd.restitution=0.5f; fd.friction=1.0f;
                    body->CreateFixture(&fd);
                    body->ApplyLinearImpulse(b2Vec2((10-rand()%21)*0.05f,0),body->GetWorldCenter(),true);
                    Ball ball; ball.body=body; ball.radius=radius; ball.orbIdx=rand()%NUM_ORBS; ball.isPlayer=false;
                    balls.push_back(ball);
                }
            }
            // player cube
            if(globalTime==50*dropTime){
                b2BodyDef bd; bd.type=b2_dynamicBody;
                bd.position.Set((float)W*0.5f/PPM,-400.0f/PPM);
                b2Body* body=world.CreateBody(&bd);
                b2PolygonShape ps; ps.SetAsBox(PLAYER_SIZE*0.5f/PPM,PLAYER_SIZE*0.5f/PPM);
                b2FixtureDef fd; fd.shape=&ps; fd.density=1.0f; fd.restitution=0.5f; fd.friction=0.7f;
                body->CreateFixture(&fd);
                Ball ball; ball.body=body; ball.radius=PLAYER_SIZE*0.5f; ball.orbIdx=0; ball.isPlayer=true;
                balls.push_back(ball);
            }

            if(!fillingDone&&globalTime>NUM_BALLS*dropTime+200){
                fillingDone=true;draining=true;
                world.DestroyBody(wallBottom);
            }
            if(draining){
                bool allOff=true;
                for(auto& b:balls) if(b.body->GetPosition().y*PPM<H+300){allOff=false;break;}
                if(allOff){simRunning=false;firstSim=false;}
            }

            // physics
            Uint32 now=SDL_GetTicks();
            physAccum+=(now-lastTick)/1000.0f; lastTick=now;
            while(physAccum>=physStep){world.Step(physStep,8,3);physAccum-=physStep;}

            // draw background
            int bm=g_settings.bg_mode;
            if(bm==BG_IMAGE&&bgTex.ok){
                glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
                drawBgImage(bgTex,W,H);
            } else if(bm==BG_COLOR){
                float r=GetRValue(g_settings.bg_color)/255.0f;
                float g2=GetGValue(g_settings.bg_color)/255.0f;
                float b2=GetBValue(g_settings.bg_color)/255.0f;
                glClearColor(r,g2,b2,1);glClear(GL_COLOR_BUFFER_BIT);
            } else {
                glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
            }

            // walls are invisible - physics only

            // balls
            for(auto& b:balls){
                float px=b.body->GetPosition().x*PPM;
                float py=b.body->GetPosition().y*PPM;
                float ang=b.body->GetAngle()*180.0f/(float)M_PI;
                if(b.isPlayer){
                    float s=PLAYER_SIZE;
                    if(cubeTex.ok) drawTexturedQuad(cubeTex.id,px,py,s,s,ang);
                    else{
                        glColor3f(0.78f,0.39f,0.39f);
                        glPushMatrix();glTranslatef(px,py,0);glRotatef(-ang,0,0,1);
                        float h2=s/2;
                        glBegin(GL_QUADS);glVertex2f(-h2,-h2);glVertex2f(h2,-h2);glVertex2f(h2,h2);glVertex2f(-h2,h2);glEnd();
                        glPopMatrix();glColor3f(1,1,1);
                    }
                } else {
                    float d=b.radius*2;
                    if(orbTex[b.orbIdx].ok) drawTexturedQuad(orbTex[b.orbIdx].id,px,py,d,d,ang);
                    else drawCircleFallback(px,py,b.radius);
                }
            }

            SDL_GL_SwapWindow(win);

            // frame cap (vsync handles it but just in case)
            Uint32 elapsed=SDL_GetTicks()-now;
            Uint32 target=1000/fps;
            if(elapsed<target) SDL_Delay(target-elapsed);
        }
        balls.clear();
    }

    for(int i=0;i<NUM_ORBS;i++) if(orbTex[i].ok)glDeleteTextures(1,&orbTex[i].id);
    if(cubeTex.ok)glDeleteTextures(1,&cubeTex.id);
    if(bgTex.ok)glDeleteTextures(1,&bgTex.id);
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    IMG_Quit();SDL_Quit();
}

// ── Entry point ───────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR cmdLine,int){
    timeBeginPeriod(1);
    loadCfg();

    // parse args
    int argc; LPWSTR* wargv=CommandLineToArgvW(GetCommandLineW(),&argc);
    bool doConfig=false,doPreview=false,doRun=false;
    HWND previewHwnd=nullptr;
    for(int i=1;i<argc;i++){
        char a[64]; WideCharToMultiByte(CP_ACP,0,wargv[i],-1,a,sizeof(a),0,0);
        for(char* p=a;*p;p++) *p=tolower(*p);
        if(!strcmp(a,"/s")||!strcmp(a,"-s")) doRun=true;
        else if(!strncmp(a,"/c",2)||!strncmp(a,"-c",2)) doConfig=true;
        else if(!strcmp(a,"/p")||!strcmp(a,"-p")){
            doPreview=true;
            if(i+1<argc){char b[32];WideCharToMultiByte(CP_ACP,0,wargv[i+1],-1,b,sizeof(b),0,0);previewHwnd=(HWND)(uintptr_t)atoll(b);}
        }
    }
    LocalFree(wargv);

    if(doConfig){ bool prev=runWin32Settings(); if(prev)runScreensaver(false,nullptr); }
    else if(doPreview) runScreensaver(true,(void*)previewHwnd);
    else runScreensaver(false,nullptr);

    timeEndPeriod(1);
    return 0;
}
