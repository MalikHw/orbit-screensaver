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

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <commdlg.h>
#include <commctrl.h>
#include <timeapi.h>
#include <dwmapi.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dwmapi.lib")
#else
#include <unistd.h>
#endif
#include <SDL2/SDL_syswm.h>

// settings path
#ifdef _WIN32
#define CFG_FILE "orbit_screensaver.ini"
static std::string getCfgPath() {
    char path[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path);
    return std::string(path) + "\\" + CFG_FILE;
}
#else
static std::string getCfgPath() {
    const char* home = getenv("HOME");
    return std::string(home ? home : ".") + "/.config/orbit_screensaver.ini";
}
#endif

#define FPS 60
#define TICK_RATE 60
#define NUM_BALLS 120
#define DROP_TIME 20
#define PLAYER_SIZE 80
#define NUM_ORBS 10
#define PPM 40.0f // pixels per meter for box2d

// bg modes
#define BG_BLACK 0
#define BG_TRANSPARENT 1
#define BG_TINT 2
#define BG_FADE 3

struct Settings {
    int speed;
    int bg_mode;
    char cube_path[512];
};

static Settings g_settings = { 10, BG_BLACK, "" };

// lol
static void loadCfg() {
    FILE* f = fopen(getCfgPath().c_str(), "r");
    if (!f) return;
    char line[640];
    while (fgets(line, sizeof(line), f)) {
        int iv; char sv[512];
        if (sscanf(line, "speed=%d", &iv) == 1) g_settings.speed = iv;
        if (sscanf(line, "bg_mode=%d", &iv) == 1) g_settings.bg_mode = iv;
        if (sscanf(line, "cube_path=%511[^\n]", sv) == 1) strncpy(g_settings.cube_path, sv, 511);
    }
    fclose(f);
}

static void saveCfg() {
    // make dir if needed on linux
#ifndef _WIN32
    std::string dir = getCfgPath();
    size_t slash = dir.rfind('/');
    if (slash != std::string::npos) {
        std::string d = dir.substr(0, slash);
        char cmd[600]; snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", d.c_str());
        system(cmd); // sue me
    }
#endif
    FILE* f = fopen(getCfgPath().c_str(), "w");
    if (!f) return;
    fprintf(f, "speed=%d\n", g_settings.speed);
    fprintf(f, "bg_mode=%d\n", g_settings.bg_mode);
    fprintf(f, "cube_path=%s\n", g_settings.cube_path);
    fclose(f);
}

// terminal config for linux (and windows i guess whatever)
static void runConfigTerminal() {
    loadCfg();
    printf("=== orbit screensaver config ===\n");

    printf("speed (1-20) [current: %d]: ", g_settings.speed);
    fflush(stdout);
    char buf[64]; 
    if (fgets(buf, sizeof(buf), stdin) && buf[0] != '\n') {
        int v = atoi(buf);
        if (v >= 1 && v <= 20) g_settings.speed = v;
    }

    printf("cube path [current: %s]: ", g_settings.cube_path[0] ? g_settings.cube_path : "none");
    fflush(stdout);
    char pbuf[512];
    if (fgets(pbuf, sizeof(pbuf), stdin) && pbuf[0] != '\n') {
        pbuf[strcspn(pbuf, "\n")] = 0;
        strncpy(g_settings.cube_path, pbuf, 511);
    }

    printf("bg mode - 0=black 1=transparent 2=tint 3=fade [current: %d]: ", g_settings.bg_mode);
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin) && buf[0] != '\n') {
        int v = atoi(buf);
        if (v >= 0 && v <= 3) g_settings.bg_mode = v;
    }

    saveCfg();
    printf("saved. bye\n");
}

#ifdef _WIN32
// win32 settings dialog - pure win32 api, no mfc no atl no nothing
#define IDC_SPEED_SLIDER 101
#define IDC_SPEED_LABEL  102
#define IDC_CUBE_EDIT    103
#define IDC_CUBE_BROWSE  104
#define IDC_BG_BLACK     105
#define IDC_BG_TRANS     106
#define IDC_BG_TINT      107
#define IDC_BG_FADE      108
#define IDC_SAVE         109
#define IDC_PREVIEW      110

static bool g_preview_clicked = false;

static LRESULT CALLBACK SettingsDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch(msg) {
    case WM_CREATE: {
        HFONT font = CreateFontA(16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Consolas");

        auto mkLabel = [&](const char* t, int x, int y, int w, int h) {
            HWND l = CreateWindowA("STATIC",t,WS_CHILD|WS_VISIBLE,x,y,w,h,hwnd,0,0,0);
            SendMessage(l,WM_SETFONT,(WPARAM)font,TRUE); return l;
        };
        auto mkBtn = [&](const char* t, int id, int x, int y, int w, int h) {
            HWND b = CreateWindowA("BUTTON",t,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,x,y,w,h,hwnd,(HMENU)(intptr_t)id,0,0);
            SendMessage(b,WM_SETFONT,(WPARAM)font,TRUE); return b;
        };
        auto mkRadio = [&](const char* t, int id, int x, int y, int w, int h, bool first) {
            DWORD style = WS_CHILD|WS_VISIBLE|BS_AUTORADIOBUTTON|(first?WS_GROUP:0);
            HWND r = CreateWindowA("BUTTON",t,style,x,y,w,h,hwnd,(HMENU)(intptr_t)id,0,0);
            SendMessage(r,WM_SETFONT,(WPARAM)font,TRUE); return r;
        };

        mkLabel("ORBIT SCREENSAVER", 20, 14, 300, 22);
        mkLabel("Speed", 20, 48, 60, 20);
        HWND slider = CreateWindowA(TRACKBAR_CLASSA,"",WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS,
            90,44,200,28,hwnd,(HMENU)IDC_SPEED_SLIDER,0,0);
        SendMessage(slider,TBM_SETRANGE,TRUE,MAKELPARAM(1,20));
        SendMessage(slider,TBM_SETPOS,TRUE,g_settings.speed);
        char spd[8]; sprintf(spd,"%d",g_settings.speed);
        HWND sl = CreateWindowA("STATIC",spd,WS_CHILD|WS_VISIBLE,298,48,30,20,hwnd,(HMENU)IDC_SPEED_LABEL,0,0);
        SendMessage(sl,WM_SETFONT,(WPARAM)font,TRUE);

        mkLabel("cube.png", 20, 84, 60, 20);
        HWND ed = CreateWindowA("EDIT",g_settings.cube_path,
            WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,90,82,190,22,hwnd,(HMENU)IDC_CUBE_EDIT,0,0);
        SendMessage(ed,WM_SETFONT,(WPARAM)font,TRUE);
        mkBtn("...", IDC_CUBE_BROWSE, 286, 82, 30, 22);

        mkLabel("Background", 20, 118, 80, 20);
        mkRadio("Black",       IDC_BG_BLACK, 110, 114, 100, 20, true);
        mkRadio("Transparent", IDC_BG_TRANS, 110, 136, 100, 20, false);
        mkRadio("Dark tint",   IDC_BG_TINT,  110, 158, 100, 20, false);
        mkRadio("Fade to black",IDC_BG_FADE, 110, 180, 120, 20, false);

        // check correct radio
        int radioIds[] = {IDC_BG_BLACK,IDC_BG_TRANS,IDC_BG_TINT,IDC_BG_FADE};
        CheckDlgButton(hwnd, radioIds[g_settings.bg_mode], BST_CHECKED);

        mkBtn("Save",    IDC_SAVE,    90,  212, 80, 28);
        mkBtn("Preview", IDC_PREVIEW, 180, 212, 80, 28);

        // credits - links via static with hand cursor, handle in WM_CTLCOLORSTATIC
        mkLabel("by ", 70, 250, 25, 18);
        HWND lMalik = CreateWindowA("BUTTON","MalikHw47",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,94,248,72,20,hwnd,(HMENU)201,0,0);
        SendMessage(lMalik,WM_SETFONT,(WPARAM)font,TRUE);
        mkLabel(" - ", 168,250,18,18);
        HWND lYT = CreateWindowA("BUTTON","youtube",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,186,248,58,20,hwnd,(HMENU)202,0,0);
        SendMessage(lYT,WM_SETFONT,(WPARAM)font,TRUE);
        mkLabel(" - ", 246,250,18,18);
        HWND lGH = CreateWindowA("BUTTON","github",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,264,248,52,20,hwnd,(HMENU)203,0,0);
        SendMessage(lGH,WM_SETFONT,(WPARAM)font,TRUE);

        return 0;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* di = (DRAWITEMSTRUCT*)lp;
        SetBkMode(di->hDC, TRANSPARENT);
        SetTextColor(di->hDC, RGB(85,136,255));
        char txt[64]; GetWindowTextA(di->hwndItem, txt, sizeof(txt));
        DrawTextA(di->hDC, txt, -1, &di->rcItem, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        if (di->itemState & ODS_FOCUS) DrawFocusRect(di->hDC, &di->rcItem);
        return TRUE;
    }
    case WM_SETCURSOR: {
        int id = GetDlgCtrlID((HWND)wp);
        if (id==201||id==202||id==203) { SetCursor(LoadCursor(0,IDC_HAND)); return TRUE; }
        return DefWindowProcA(hwnd,msg,wp,lp);
    }
    case WM_HSCROLL: {
        if ((HWND)lp == GetDlgItem(hwnd,IDC_SPEED_SLIDER)) {
            int v = (int)SendDlgItemMessage(hwnd,IDC_SPEED_SLIDER,TBM_GETPOS,0,0);
            char s[8]; sprintf(s,"%d",v);
            SetDlgItemTextA(hwnd,IDC_SPEED_LABEL,s);
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == IDC_CUBE_BROWSE) {
            OPENFILENAMEA ofn = {};
            char buf[512] = "";
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = "PNG files\0*.png\0All\0*.*\0";
            ofn.lpstrFile = buf;
            ofn.nMaxFile = sizeof(buf);
            ofn.Flags = OFN_FILEMUSTEXIST;
            if (GetOpenFileNameA(&ofn)) SetDlgItemTextA(hwnd, IDC_CUBE_EDIT, buf);
        }
        if (id==201) ShellExecuteA(hwnd,"open","https://malikhw.github.io",0,0,SW_SHOW);
        if (id==202) ShellExecuteA(hwnd,"open","https://youtube.com/@MalikHw47",0,0,SW_SHOW);
        if (id==203) ShellExecuteA(hwnd,"open","https://github.com/MalikHw",0,0,SW_SHOW);
        if (id==IDC_SAVE||id==IDC_PREVIEW) {
            g_settings.speed = (int)SendDlgItemMessage(hwnd,IDC_SPEED_SLIDER,TBM_GETPOS,0,0);
            char buf[512]; GetDlgItemTextA(hwnd,IDC_CUBE_EDIT,buf,sizeof(buf));
            strncpy(g_settings.cube_path,buf,511);
            int radioIds[]={IDC_BG_BLACK,IDC_BG_TRANS,IDC_BG_TINT,IDC_BG_FADE};
            for(int i=0;i<4;i++) if(IsDlgButtonChecked(hwnd,radioIds[i])) g_settings.bg_mode=i;
            saveCfg();
            if (id==IDC_PREVIEW) { g_preview_clicked=true; DestroyWindow(hwnd); }
            else MessageBoxA(hwnd,"Settings saved!","Orbit",MB_OK|MB_ICONINFORMATION);
        }
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd,msg,wp,lp);
}

static bool runWin32Settings() {
    g_preview_clicked = false;
    WNDCLASSA wc = {};
    wc.lpfnWndProc = SettingsDlgProc;
    wc.lpszClassName = "OrbitSettings";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    wc.hCursor = LoadCursor(0,IDC_ARROW);
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("OrbitSettings","Orbit Screensaver - Settings",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
        CW_USEDEFAULT,CW_USEDEFAULT,360,300,0,0,0,0);
    ShowWindow(hwnd,SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while(GetMessage(&msg,0,0,0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return g_preview_clicked;
}
#endif // _WIN32

// texture stuff
struct Texture { GLuint id; int w,h; bool ok; };

static Texture loadTexture(const char* path) {
    Texture t = {0,0,0,false};
    SDL_Surface* surf = IMG_Load(path);
    if (!surf) { fprintf(stderr,"cant load %s: %s\n",path,IMG_GetError()); return t; }
    SDL_Surface* conv = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);
    if (!conv) return t;
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,conv->w,conv->h,0,GL_RGBA,GL_UNSIGNED_BYTE,conv->pixels);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    t.w=conv->w; t.h=conv->h; t.ok=true;
    SDL_FreeSurface(conv);
    return t;
}

static void drawTexturedQuad(GLuint texId, float cx, float cy, float w, float h, float angleDeg) {
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, texId);
    glPushMatrix();
    glTranslatef(cx, cy, 0);
    glRotatef(-angleDeg, 0, 0, 1);
    float hw=w/2, hh=h/2;
    glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex2f(-hw,-hh);
    glTexCoord2f(1,0); glVertex2f( hw,-hh);
    glTexCoord2f(1,1); glVertex2f( hw, hh);
    glTexCoord2f(0,1); glVertex2f(-hw, hh);
    glEnd();
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

static void drawCircleFallback(float cx, float cy, float r) {
    glColor3f(0.39f,0.39f,0.78f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx,cy);
    for(int i=0;i<=32;i++) {
        float a = i*2*M_PI/32;
        glVertex2f(cx+cosf(a)*r, cy+sinf(a)*r);
    }
    glEnd();
    glColor3f(1,1,1);
}

// physics body info
struct Ball {
    b2Body* body;
    float radius;
    int orbIdx;
    bool isPlayer;
};

static std::string getExeDir() {
#ifdef _WIN32
    char buf[MAX_PATH]; GetModuleFileNameA(0,buf,MAX_PATH);
    std::string s(buf); return s.substr(0,s.rfind('\\'));
#else
    char buf[4096]; ssize_t n=readlink("/proc/self/exe",buf,sizeof(buf)-1);
    if(n>0){buf[n]=0;std::string s(buf);return s.substr(0,s.rfind('/'));}
    return ".";
#endif
}

// the actual screensaver loop
static void runScreensaver(bool isPreview, void* previewHandle) {
#ifdef _WIN32
    // MUST set SDL_WINDOWID before SDL_Init or embedding wont work
    HWND parentHwnd = (HWND)previewHandle;
    if(isPreview && parentHwnd) {
        char envbuf[128];
        sprintf(envbuf, "SDL_VIDEODRIVER=windib");
        putenv(envbuf);
        sprintf(envbuf, "SDL_WINDOWID=%llu", (unsigned long long)(uintptr_t)parentHwnd);
        putenv(envbuf);
    }
#endif
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0){fprintf(stderr,"sdl died lmao\n");return;}
    IMG_Init(IMG_INIT_PNG);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,0);

    int W,H;
    SDL_Window* win;
    Uint32 winFlags = SDL_WINDOW_OPENGL;

    if(isPreview) {
#ifdef _WIN32
        // get actual size of the preview box
        RECT rc; GetClientRect(parentHwnd, &rc);
        W = rc.right  - rc.left; if(W<=0) W=152;
        H = rc.bottom - rc.top;  if(H<=0) H=112;
#else
        W=152; H=112;
#endif
        win = SDL_CreateWindow("orbit",0,0,W,H,winFlags);
    } else {
        SDL_DisplayMode dm; SDL_GetCurrentDisplayMode(0,&dm);
        W=dm.w; H=dm.h;
        win = SDL_CreateWindow("orbit",0,0,W,H,winFlags|SDL_WINDOW_FULLSCREEN_DESKTOP|SDL_WINDOW_BORDERLESS);
        SDL_ShowCursor(SDL_DISABLE);
    }

    if(!win){fprintf(stderr,"window said no\n");SDL_Quit();return;}
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1); // vsync

    // ortho projection, y down
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0,W,H,0,-1,1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);

    // load textures
    std::string exeDir = getExeDir();
    Texture orbTex[NUM_ORBS];
    for(int i=0;i<NUM_ORBS;i++){
        char path[600]; snprintf(path,sizeof(path),"%s/orb%d.png",exeDir.c_str(),i+1);
        orbTex[i] = loadTexture(path);
    }
    Texture cubeTex = {0,0,0,false};
    const char* cubeSrc = g_settings.cube_path[0] ? g_settings.cube_path : nullptr;
    if(!cubeSrc){
        char path[600]; snprintf(path,sizeof(path),"%s/cube.png",exeDir.c_str());
        cubeTex = loadTexture(path);
    } else cubeTex = loadTexture(cubeSrc);

    // win32 transparency via DWM - actually works with opengl unlike colorkey lmao
#ifdef _WIN32
    bool useTransparency = !isPreview && (g_settings.bg_mode==BG_TRANSPARENT||g_settings.bg_mode==BG_TINT||g_settings.bg_mode==BG_FADE);
    HWND sdlHwnd = nullptr;
    if(useTransparency) {
        SDL_SysWMinfo wmi; SDL_VERSION(&wmi.version);
        if(SDL_GetWindowWMInfo(win,&wmi)) sdlHwnd=wmi.info.win.window;
        if(sdlHwnd) {
            // DWM blur behind = real compositor transparency
            DWM_BLURBEHIND bb = {};
            bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
            bb.fEnable = TRUE;
            HRGN rgn = CreateRectRgn(0,0,-1,-1); // empty region = full window
            bb.hRgnBlur = rgn;
            DwmEnableBlurBehindWindow(sdlHwnd, &bb);
            DeleteObject(rgn);
            // also set WS_EX_LAYERED for fade mode alpha control
            if(g_settings.bg_mode==BG_FADE) {
                LONG style = GetWindowLong(sdlHwnd,GWL_EXSTYLE);
                SetWindowLong(sdlHwnd,GWL_EXSTYLE,style|WS_EX_LAYERED);
                SetLayeredWindowAttributes(sdlHwnd,0,0,LWA_ALPHA); // start invisible
            }
        }
    }
#endif

    srand((unsigned)time(nullptr));
    bool running = true;
    bool firstSim = true;

    while(running) {
        // new simulation
        unsigned int seed = (unsigned)rand();
        srand(seed);

        float speedMult = g_settings.speed / 10.0f;
        b2Vec2 gravity(0.0f, 9.8f * speedMult * 3.0f); // scaled up since our world is big
        b2World world(gravity);

        // ground + walls as static bodies
        auto makeWall = [&](float x1,float y1,float x2,float y2){
            b2BodyDef bd; bd.type=b2_staticBody;
            b2Body* b=world.CreateBody(&bd);
            b2EdgeShape es; es.SetTwoSided(b2Vec2(x1/PPM,y1/PPM),b2Vec2(x2/PPM,y2/PPM));
            b2FixtureDef fd; fd.shape=&es; fd.restitution=0.5f; fd.friction=0.7f;
            b->CreateFixture(&fd);
            return b;
        };
        b2Body* wallBottom = makeWall(0,H,W,H);
        b2Body* wallLeft   = makeWall(0,0,0,H);
        b2Body* wallRight  = makeWall(W,0,W,H);
        (void)wallLeft; (void)wallRight;

        std::vector<Ball> balls;
        int globalTime = 0;
        bool fillingDone = false;
        bool draining = false;
        bool fadeDone = !firstSim;
        int fadeTick = 0;
        int fadeFrames = TICK_RATE / 2; // 0.5 sec

        SDL_Point lastMouse; SDL_GetMouseState(&lastMouse.x,&lastMouse.y);
        int grace = 60;

        Uint32 lastTick = SDL_GetTicks();
        float physAccum = 0.0f;
        const float physStep = 1.0f/FPS;

        bool simRunning = true;
        while(simRunning && running) {
            globalTime++;

            SDL_Event ev;
            while(SDL_PollEvent(&ev)) {
                if(ev.type==SDL_QUIT){running=false;simRunning=false;}
                if(globalTime>grace && !isPreview) {
                    if(ev.type==SDL_KEYDOWN||ev.type==SDL_MOUSEBUTTONDOWN){running=false;simRunning=false;}
                    if(ev.type==SDL_MOUSEMOTION&&(ev.motion.x!=lastMouse.x||ev.motion.y!=lastMouse.y)){running=false;simRunning=false;}
                }
            }
#ifdef _WIN32
            // die when parent preview window closes
            if(isPreview && parentHwnd && !IsWindow(parentHwnd)){running=false;simRunning=false;}
#endif

            // spawn balls
            for(int i=0;i<NUM_BALLS;i++){
                if(globalTime==DROP_TIME*i){
                    float radius = 40 + rand()%20;
                    b2BodyDef bd; bd.type=b2_dynamicBody;
                    bd.position.Set(((600.0f/NUM_BALLS)*(1+rand()%( NUM_BALLS*2)))/PPM, -250.0f/PPM);
                    b2Body* body=world.CreateBody(&bd);
                    b2CircleShape cs; cs.m_radius=radius/PPM;
                    b2FixtureDef fd; fd.shape=&cs; fd.density=1.0f; fd.restitution=0.5f; fd.friction=1.0f;
                    body->CreateFixture(&fd);
                    body->ApplyLinearImpulse(b2Vec2((10-rand()%21)*0.05f,0),body->GetWorldCenter(),true);
                    Ball ball; ball.body=body; ball.radius=radius; ball.orbIdx=rand()%NUM_ORBS; ball.isPlayer=false;
                    balls.push_back(ball);
                }
            }

            // spawn player cube
            if(globalTime==50*DROP_TIME){
                b2BodyDef bd; bd.type=b2_dynamicBody;
                bd.position.Set(600.0f/PPM,-400.0f/PPM);
                b2Body* body=world.CreateBody(&bd);
                b2PolygonShape ps; ps.SetAsBox(PLAYER_SIZE*0.5f/PPM,PLAYER_SIZE*0.5f/PPM);
                b2FixtureDef fd; fd.shape=&ps; fd.density=1.0f; fd.restitution=0.5f; fd.friction=0.7f;
                body->CreateFixture(&fd);
                Ball ball; ball.body=body; ball.radius=PLAYER_SIZE*0.5f; ball.orbIdx=0; ball.isPlayer=true;
                balls.push_back(ball);
            }

            // filling done? drop walls
            if(!fillingDone && globalTime > NUM_BALLS*DROP_TIME+200){
                fillingDone=true; draining=true;
                world.DestroyBody(wallBottom);
                // side walls stay or not? destroy em too for full drain
                // actually nah let em fall straight down, looks cleaner
            }

            // check all offscreen
            if(draining){
                bool allOff=true;
                for(auto& b:balls){
                    if(b.body->GetPosition().y*PPM < H+300){allOff=false;break;}
                }
                if(allOff){simRunning=false;firstSim=false;}
            }

            // physics step
            Uint32 now=SDL_GetTicks();
            physAccum += (now-lastTick)/1000.0f;
            lastTick=now;
            while(physAccum>=physStep){
                world.Step(physStep,8,3);
                physAccum-=physStep;
            }

            // fade logic
#ifdef _WIN32
            if(g_settings.bg_mode==BG_FADE && !fadeDone && sdlHwnd) {
                fadeTick++;
                int alpha=fadeTick*255/fadeFrames; if(alpha>255)alpha=255;
                SetLayeredWindowAttributes(sdlHwnd,0,alpha,LWA_ALPHA);
                if(fadeTick>=fadeFrames)fadeDone=true;
            }
#endif

            // draw
            int bm=g_settings.bg_mode;
            if(bm==BG_TRANSPARENT){
                // alpha=0 + DWM blur behind = desktop shows through
                glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
            } else if(bm==BG_TINT){
                glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
                glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
                glColor4f(0,0,0,0.47f);
                glBegin(GL_QUADS); glVertex2f(0,0);glVertex2f(W,0);glVertex2f(W,H);glVertex2f(0,H); glEnd();
                glDisable(GL_BLEND); glColor4f(1,1,1,1);
            } else {
                glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
            }

            // walls
            if(!draining){
                glColor3f(0.31f,0.31f,0.31f);
                glLineWidth(5);
                glBegin(GL_LINES);
                glVertex2f(0,H);glVertex2f(W,H);
                glVertex2f(0,0);glVertex2f(0,H);
                glVertex2f(W,0);glVertex2f(W,H);
                glEnd();
                glColor3f(1,1,1);
            }

            // bodies
            for(auto& b:balls){
                float px=b.body->GetPosition().x*PPM;
                float py=b.body->GetPosition().y*PPM;
                float angleDeg=b.body->GetAngle()*180.0f/M_PI;
                if(b.isPlayer){
                    float s=PLAYER_SIZE;
                    if(cubeTex.ok) drawTexturedQuad(cubeTex.id,px,py,s,s,angleDeg);
                    else {
                        glColor3f(0.78f,0.39f,0.39f);
                        glPushMatrix(); glTranslatef(px,py,0); glRotatef(-angleDeg,0,0,1);
                        float h=s/2;
                        glBegin(GL_QUADS);glVertex2f(-h,-h);glVertex2f(h,-h);glVertex2f(h,h);glVertex2f(-h,h);glEnd();
                        glPopMatrix(); glColor3f(1,1,1);
                    }
                } else {
                    float r=b.radius*2;
                    if(orbTex[b.orbIdx].ok) drawTexturedQuad(orbTex[b.orbIdx].id,px,py,r,r,angleDeg);
                    else drawCircleFallback(px,py,b.radius);
                }
            }

            SDL_GL_SwapWindow(win);

            // cap to TICK_RATE
            Uint32 elapsed=SDL_GetTicks()-now;
            Uint32 target=1000/TICK_RATE;
            if(elapsed<target) SDL_Delay(target-elapsed);
        }

        // cleanup world (bodies auto cleaned with world)
        balls.clear();
    }

    // cleanup textures
    for(int i=0;i<NUM_ORBS;i++) if(orbTex[i].ok) glDeleteTextures(1,&orbTex[i].id);
    if(cubeTex.ok) glDeleteTextures(1,&cubeTex.id);

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    IMG_Quit();
    SDL_Quit();
}

int main(int argc, char** argv) {
#ifdef _WIN32
    timeBeginPeriod(1);
#endif
    loadCfg();

#ifdef _WIN32
    // parse windows screensaver args
    bool doSave=false, doPreview=false, doConfig=false;
    HWND previewHwnd=nullptr;
    for(int i=1;i<argc;i++){
        std::string a(argv[i]);
        for(auto& c:a) c=tolower(c);
        if(a=="/s"||a=="-s") doSave=true;
        else if(a=="/c"||a=="-c"||a.substr(0,3)=="/c:") doConfig=true;
        else if(a=="/p"||a=="-p") {
            doPreview=true;
            if(i+1<argc) previewHwnd=(HWND)(uintptr_t)atoll(argv[i+1]);
        }
    }
    if(doConfig){ bool preview=runWin32Settings(); if(preview) runScreensaver(false,nullptr); }
    else if(doPreview) runScreensaver(true,(void*)previewHwnd);
    else runScreensaver(false,nullptr); // /s or bare launch
#else
    // linux args
    bool doConfig=false;
    for(int i=1;i<argc;i++){
        std::string a(argv[i]);
        if(a=="--configure"||a=="-c") doConfig=true;
    }
    if(doConfig) runConfigTerminal();
    else runScreensaver(false,nullptr);
#endif
#ifdef _WIN32
    timeEndPeriod(1);
#endif
    return 0;
}
