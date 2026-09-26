Unicode True
Name "REX Player"
OutFile "release\\REX-Player-Windows-x64-setup.exe"
InstallDir "$PROGRAMFILES64\\REX Player"
InstallDirRegKey HKLM "Software\\REX Player" "InstallDir"
RequestExecutionLevel admin
ManifestSupportedOS "win10"
Icon "assets\\rex-player.ico"
UninstallIcon "assets\\rex-player.ico"
BrandingText "REX Player"

!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "assets\\rex-player.ico"
!define MUI_UNICON "assets\\rex-player.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\\rex-player.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch REX Player"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Section "REX Player" SecMain
  SectionIn RO

  SetOutPath "$INSTDIR"
  File /r "dist\\bin\\*"

  WriteRegStr HKLM "Software\\REX Player" "InstallDir" "$INSTDIR"

  CreateDirectory "$SMPROGRAMS\\REX Player"
  CreateShortcut "$SMPROGRAMS\\REX Player\\REX Player.lnk" "$INSTDIR\\rex-player.exe"
  CreateShortcut "$DESKTOP\\REX Player.lnk" "$INSTDIR\\rex-player.exe"

  WriteUninstaller "$INSTDIR\\Uninstall.exe"
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\\REX Player.lnk"
  Delete "$SMPROGRAMS\\REX Player\\REX Player.lnk"
  RMDir "$SMPROGRAMS\\REX Player"

  DeleteRegKey HKLM "Software\\REX Player"

  RMDir /r "$INSTDIR"
SectionEnd
