; NOTE: don't forget to update the version (don't miss releaseYYMMDD.txt)

[Setup]
AppName=Suneido
AppVerName=Suneido Version 1.071015
AppVersion=1.071015
AppPublisher=Suneido Software
AppPublisherURL=http://www.suneido.com
AppSupportURL=http://www.suneido.com
AppUpdatesURL=http://www.suneido.com
DefaultDirName=c:\Suneido
DefaultGroupName=Suneido
DisableProgramGroupPage=yes
LicenseFile=C:\dev\suneido\COPYING
OutputBaseFilename=SuneidoSetup071015
; cosmetic
WizardImageBackColor=clWhite
WizardImageFile=suneidoLarge.bmp
WizardSmallImageFile=suneidoSmall.bmp

; uncomment the following line if you want your installation to run on NT 3.51 too.
; MinVersion=4,3.51

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; MinVersion: 4,4

[Files]
Source: "c:\suneido071015\suneido.exe"; DestDir: "{app}"; Flags: ignoreversion
;Source: "c:\suneido071015\suneido.exe"; DestDir: "{app}"; DestName: "suneido_new.exe"; Flags: ignoreversion
Source: "c:\suneido071015\*.*"; DestDir: "{app}"; Excludes: suneido.exe, suneido.db; Flags: ignoreversion recursesubdirs createallsubdirs

[INI]
Filename: "{app}\suneido.url"; Section: "InternetShortcut"; Key: "URL"; String: "http://www.suneido.com"

[Icons]
Name: "{group}\Suneido"; Filename: "{app}\suneido.exe"; WorkingDir: {app};
Name: "{group}\Suneido on the Web"; Filename: "{app}\suneido.url"; WorkingDir: {app};
Name: "{userdesktop}\Suneido"; Filename: "{app}\suneido.exe"; MinVersion: 4,4; Tasks: desktopicon; WorkingDir: {app};

[Messages]
WelcomeLabel1=[name] Setup
WelcomeLabel2=Copyright © 2000-2007 Suneido Software%nAll rights reserved worldwide.

[Run]
;===================
;Filename: "{app}\update.bat"; StatusMsg: "Converting database..."; Flags: runhidden
;===================
Filename: "{app}\suneido.exe"; Parameters: "-load stdlib"; StatusMsg: "Load Standard Library..."
Filename: "{app}\suneido.exe"; Parameters: "-load suneidoc"; StatusMsg: "Load Users Manual..."
Filename: "{app}\suneido.exe"; Parameters: "-load imagebook"; StatusMsg: "Load Image Book..."
Filename: "{app}\suneido.exe"; Parameters: "-load translatelanguage"; StatusMsg: "Load Language Translations..."
Filename: "{app}\suneido.exe"; Description: "Launch Suneido"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: files; Name: "{app}\suneido.url"
Type: files; Name: "{app}\suneido.db"
Type: files; Name: "{app}\suneido.exe"
