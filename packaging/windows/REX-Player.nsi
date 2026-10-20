Unicode True
Name "REX Player"
OutFile "..\\..\\release\\REX-Player-Windows-x64-setup.exe"
InstallDir "$PROGRAMFILES64\\REX Player"
InstallDirRegKey HKLM "Software\\REX Player" "InstallDir"
RequestExecutionLevel admin
ManifestSupportedOS "win10"
Icon "..\\..\\assets\\rex-player.ico"
UninstallIcon "..\\..\\assets\\rex-player.ico"
BrandingText "REX Player"

!include "MUI2.nsh"

!define MUI_ABORTWARNING
!define MUI_ICON "..\\..\\assets\\rex-player.ico"
!define MUI_UNICON "..\\..\\assets\\rex-player.ico"
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
  File /r "..\\..\\dist\\bin\\*"

  WriteRegStr HKLM "Software\\REX Player" "InstallDir" "$INSTDIR"

  ; Register REX Player with Windows Apps & Features / Programs and Features.
  ; Without this uninstall registry entry, Windows has no registered
  ; uninstall command to invoke from Settings or Control Panel.
  SetRegView 64
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\REX Player" "DisplayName" "REX Player"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\REX Player" "DisplayVersion" "5.0.1"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\REX Player" "Publisher" "ipSuman"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\REX Player" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\REX Player" "DisplayIcon" "$INSTDIR\\rex-player.exe"
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\REX Player" "UninstallString" '"$INSTDIR\\Uninstall.exe"'
  WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\REX Player" "QuietUninstallString" '"$INSTDIR\\Uninstall.exe" /S'
  WriteRegDWORD HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\REX Player" "NoModify" 1
  WriteRegDWORD HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\REX Player" "NoRepair" 1

  ; Register REX Player in Windows Default Apps and Open With.
  ; This offers supported file types without overriding the user's defaults.
  WriteRegStr HKLM "Software\\Classes\\Applications\\rex-player.exe\\shell\\open\\command" "" '"$INSTDIR\\rex-player.exe" "%1"'
  WriteRegStr HKLM "Software\\Classes\\Applications\\rex-player.exe" "FriendlyAppName" "REX Player"
  WriteRegStr HKLM "Software\\Classes\\REXPlayer.MediaFile" "" "REX Player Media File"
  WriteRegStr HKLM "Software\\Classes\\REXPlayer.MediaFile\\DefaultIcon" "" "$INSTDIR\\rex-player.exe,0"
  WriteRegStr HKLM "Software\\Classes\\REXPlayer.MediaFile\\shell\\open\\command" "" '"$INSTDIR\\rex-player.exe" "%1"'

  WriteRegStr HKLM "Software\\REX Player\\Capabilities" "ApplicationName" "REX Player"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities" "ApplicationDescription" "Play video and audio files with REX Player"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities" "ApplicationIcon" "$INSTDIR\\rex-player.exe,0"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".mp4" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".mkv" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".webm" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".avi" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".mov" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".wmv" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".m4v" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".mpeg" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".mpg" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".ts" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".mp3" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".flac" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".wav" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".m4a" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".ogg" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\REX Player\\Capabilities\\FileAssociations" ".opus" "REXPlayer.MediaFile"
  WriteRegStr HKLM "Software\\RegisteredApplications" "REX Player" "Software\\REX Player\\Capabilities"

  !macro RegisterMediaExtension EXT
    WriteRegStr HKLM "Software\\Classes\\\${EXT}\\OpenWithProgids" "REXPlayer.MediaFile" ""
    WriteRegStr HKLM "Software\\Classes\\Applications\\rex-player.exe\\SupportedTypes" "\${EXT}" ""
  !macroend
  !insertmacro RegisterMediaExtension ".mp4"
  !insertmacro RegisterMediaExtension ".mkv"
  !insertmacro RegisterMediaExtension ".webm"
  !insertmacro RegisterMediaExtension ".avi"
  !insertmacro RegisterMediaExtension ".mov"
  !insertmacro RegisterMediaExtension ".wmv"
  !insertmacro RegisterMediaExtension ".m4v"
  !insertmacro RegisterMediaExtension ".mpeg"
  !insertmacro RegisterMediaExtension ".mpg"
  !insertmacro RegisterMediaExtension ".ts"
  !insertmacro RegisterMediaExtension ".mp3"
  !insertmacro RegisterMediaExtension ".flac"
  !insertmacro RegisterMediaExtension ".wav"
  !insertmacro RegisterMediaExtension ".m4a"
  !insertmacro RegisterMediaExtension ".ogg"
  !insertmacro RegisterMediaExtension ".opus"

  CreateDirectory "$SMPROGRAMS\\REX Player"
  CreateShortcut "$SMPROGRAMS\\REX Player\\REX Player.lnk" "$INSTDIR\\rex-player.exe"
  CreateShortcut "$DESKTOP\\REX Player.lnk" "$INSTDIR\\rex-player.exe"

  WriteUninstaller "$INSTDIR\\Uninstall.exe"
SectionEnd

Section "Uninstall"
  SetRegView 64

  Delete "$DESKTOP\\REX Player.lnk"
  Delete "$SMPROGRAMS\\REX Player\\REX Player.lnk"
  RMDir "$SMPROGRAMS\\REX Player"

  DeleteRegKey HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\REX Player"
  DeleteRegKey HKLM "Software\\REX Player"

  RMDir /r "$INSTDIR"
SectionEnd
