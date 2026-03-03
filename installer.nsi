!include "MUI2.nsh"
!include "LogicLib.nsh"

Name "Orbit Screensaver"
OutFile "orbit-setup.exe"
InstallDir "$LOCALAPPDATA\orbit"
RequestExecutionLevel admin

Var MesaAnswer

!define MUI_ABORTWARNING

Page custom DonationPage
Page custom MesaPage MesaPageLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function DonationPage
    nsDialogs::Create 1044
    Pop $0

    ${NSD_CreateLabel} 0 0 100% 24u "Ts so hard to make so a donation would be a W!"
    Pop $0
    ${NSD_CreateLabel} 0 26u 100% 16u "if cant its ok :)"
    Pop $0

    ${NSD_CreateLink} 0 52u 100% 14u "Gift me MegaHack (discord: malikhw)"
    Pop $0
    ${NSD_OnClick} $0 OpenMegaHack

    ${NSD_CreateLink} 0 70u 100% 14u "Get me a gift (Throne wishlist)"
    Pop $0
    ${NSD_OnClick} $0 OpenThrone

    ${NSD_CreateLink} 0 88u 100% 14u "Join the Discord server"
    Pop $0
    ${NSD_OnClick} $0 OpenDiscord

    ${NSD_CreateLink} 0 106u 100% 14u "Ko-fi donation"
    Pop $0
    ${NSD_OnClick} $0 OpenKofi

    ${NSD_CreateLink} 0 124u 100% 14u "Source code on GitHub"
    Pop $0
    ${NSD_OnClick} $0 OpenSource

    nsDialogs::Show
FunctionEnd

Function OpenMegaHack
    ExecShell "open" "https://absolllute.com/store/mega_hack?gift=1"
FunctionEnd
Function OpenThrone
    ExecShell "open" "https://throne.com/MalikHw47"
FunctionEnd
Function OpenDiscord
    ExecShell "open" "https://discord.gg/G9bZ92eg2n"
FunctionEnd
Function OpenKofi
    ExecShell "open" "https://ko-fi.com/MalikHw47"
FunctionEnd
Function OpenSource
    ExecShell "open" "https://github.com/MalikHw/orbit-screensaver-cpp"
FunctionEnd

Function MesaPage
    ; skip page entirely if no mesa dll in installer dir
    IfFileExists "$EXEDIR\opengl32.dll" +2 0
        Abort

    nsDialogs::Create 1044
    Pop $0

    ${NSD_CreateLabel} 0 0 100% 40u "Is your PC a potato? (No dedicated GPU / very old integrated graphics)"
    Pop $0
    ${NSD_CreateLabel} 0 44u 100% 30u "Mesa3D adds software OpenGL - slower but works on anything."
    Pop $0
    ${NSD_CreateLabel} 0 78u 100% 20u "Include it?"
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
    SetOutPath "$LOCALAPPDATA\orbit"

    File "orbit_screensaver.exe"
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
        File /nonfatal "$EXEDIR\opengl32.dll"
    ${EndIf}

    ; stub scr to system32
    File /oname=$SYSDIR\orbit_screensaver.scr "orbit_stub.scr"

    ; registry
    WriteRegStr HKCU "Software\Orbit" "InstallDir" "$LOCALAPPDATA\orbit"
    WriteRegStr HKCU "Control Panel\Desktop" "SCRNSAVE.EXE" "$SYSDIR\orbit_screensaver.scr"

    ; open windows screensaver dialog
    Exec "control.exe desk.cpl,,@screensaver"
SectionEnd
