!include "MUI2.nsh"
!include "LogicLib.nsh"

Name "Orbit Screensaver"
OutFile "orbit-setup.exe"
InstallDir "$PROFILE\orbit"
RequestExecutionLevel admin

Var MesaAnswer

!define MUI_ABORTWARNING

Page custom MesaPage MesaPageLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function MesaPage
    nsDialogs::Create 1044
    Pop $0
    ${NSD_CreateLabel} 0 0 100% 40u "Is your PC a potato? (No dedicated GPU / very old integrated graphics)"
    Pop $0
    ${NSD_CreateLabel} 0 44u 100% 30u "Mesa3D adds software OpenGL - slower but works on anything."
    Pop $0
    nsDialogs::Show
FunctionEnd

Function MesaPageLeave
    IfFileExists "$EXEDIR\opengl32.dll" +2 0
        Return
    MessageBox MB_YESNO|MB_ICONQUESTION "Include Mesa3D software renderer?$\n(Only needed if you get a black screen or missing graphics)" IDYES DoInclude IDNO Done
    DoInclude:
        StrCpy $MesaAnswer "yes"
    Done:
FunctionEnd

Section "Install"
    ; ask about mesa3d
    MessageBox MB_YESNO|MB_ICONQUESTION "Is your PC a potato? (No dedicated GPU / very old integrated graphics)$\n$\nInclude Mesa3D software OpenGL renderer?$\n(Only needed if you get a black screen or missing graphics)" IDYES DoMesa IDNO SkipMesa
    DoMesa:
        StrCpy $MesaAnswer "yes"
    SkipMesa:

    SetOutPath "$PROFILE\orbit"

    File "orbit_screensaver.scr"
    File "SDL2.dll"
    File "SDL2_image.dll"
    File /nonfatal "libgcc_s_seh-1.dll"
    File /nonfatal "libstdc++-6.dll"
    File /nonfatal "libwinpthread-1.dll"
    File "orb1.png"
    File "orb2.png"
    File "orb3.png"
    File "orb4.png"
    File "orb5.png"
    File "orb6.png"
    File "orb7.png"
    File "orb8.png"
    File "orb9.png"
    File "orb10.png"
    File "cube.png"

    ${If} $MesaAnswer == "yes"
        inetc::get /CAPTION "Downloading Mesa3D..." /BANNER "Fetching OpenGL software renderer..." "https://github.com/MalikHw/orbit-screensaver-cpp/releases/download/mesa3d/opengl32.dll" "$PROFILE\orbit\opengl32.dll" /END
        Pop $0
        ${If} $0 != "OK"
            MessageBox MB_OK|MB_ICONEXCLAMATION "Failed to download Mesa3D: $0$\nYou can manually download opengl32.dll and place it in $PROFILE\orbit\"
        ${EndIf}
    ${EndIf}

    ; open install folder and tell user to right-click install the .scr
    MessageBox MB_OK|MB_ICONINFORMATION "Installation complete!$\n$\nThe install folder will now open.$\nRight-click 'orbit_screensaver.scr' and click 'Install' to set it as your screensaver."
    Exec 'explorer.exe "$PROFILE\orbit"'
SectionEnd
