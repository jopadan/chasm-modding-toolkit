#include <GUIConstantsEx.au3>
#include <MsgBoxConstants.au3>
#include <FileConstants.au3>
#include <WindowsConstants.au3>
#include <GuiEdit.au3>

; ==============================
; Chasm OBJ to PNG Export & Manifest + Blank Creator GUI
; ==============================
Global Const $OBJTOOL   = @ScriptDir & "\objtool.exe"
Global Const $OBJVIEWER = @ScriptDir & "\objviewer.exe"

; --- Create main window ---
Local Const $WIN_W = 800, $WIN_H = 900
GUICreate("Chasm OBJ Toolkit GUI v1.0.0 by SMR9000", $WIN_W, $WIN_H)
GUISetFont(10)

; Title & separator
Local $idTitle = GUICtrlCreateLabel("Chasm OBJ to PNG Export", 10, 10, $WIN_W - 20, 24)
GUICtrlSetFont($idTitle, 12, 800)
GUICtrlCreatePic("", 10, 40, $WIN_W - 20, 2)

; === Section 1: Export ===
Local $LX = 10, $LY = 60
Local $LABEL_W = 120, $CTRL_H = 24, $INPUT_W = 300, $BTN_W = 80, $GAP = 10

GUICtrlCreateLabel("Sprite OBJ File:", $LX, $LY, $LABEL_W, $CTRL_H)
Local $idInput  = GUICtrlCreateInput("", $LX + $LABEL_W, $LY, $INPUT_W, $CTRL_H)
Local $idBrowse = GUICtrlCreateButton("Browse...", $LX + $LABEL_W + $INPUT_W + $GAP, $LY, $BTN_W, $CTRL_H)

Local $y2 = $LY + 50
Local $idExport      = GUICtrlCreateButton("Export", $LX, $y2, 100, 30)
Local $idChkFolder   = GUICtrlCreateCheckbox("Open folder", $LX + 120, $y2 + 5, 120, 20)
Local $idChkManifest = GUICtrlCreateCheckbox("Open manifest", $LX + 120, $y2 + 30, 120, 20)
GUICtrlSetState($idChkFolder,   $GUI_CHECKED)
GUICtrlSetState($idChkManifest, $GUI_UNCHECKED)

; === Shared Manifest preview (no wrap / horiz-scroll) ===
Local $RX = $LX + $LABEL_W + $INPUT_W + $BTN_W + (2 * $GAP)
Local $RY = $LY
GUICtrlCreateLabel("Manifest Text:", $RX, $RY, 120, $CTRL_H)
Local $manW = $WIN_W - $RX - $GAP, $manH = 260
Local $idManifestEd = GUICtrlCreateEdit( _
    "", $RX, $RY + $CTRL_H + $GAP, $manW, $manH, _
    BitOR($WS_VSCROLL, $WS_HSCROLL, $ES_AUTOVSCROLL, $ES_AUTOHSCROLL, $ES_MULTILINE))
GUICtrlSetState($idManifestEd, $GUI_DISABLE)

GUICtrlCreatePic("", 10, $y2 + 90, $WIN_W - 20, 2)

; === Section 2: Manifest Creator ===
Local $y3Title = $y2 + 100
Local $idTitle2 = GUICtrlCreateLabel("Chasm OBJ Manifest Creator", $LX, $y3Title, $WIN_W - 20, 24)
GUICtrlSetFont($idTitle2, 12, 800)
GUICtrlCreatePic("", 10, $y3Title + 30, $WIN_W - 20, 2)

Local $y3 = $y3Title + 40
GUICtrlCreateLabel("Select PNG Folder:", $LX, $y3, $LABEL_W, $CTRL_H)
Local $idMan2Input  = GUICtrlCreateInput("", $LX + $LABEL_W, $y3, $INPUT_W, $CTRL_H)
Local $idMan2Browse = GUICtrlCreateButton("Browse...", $LX + $LABEL_W + $INPUT_W + $GAP, $y3, $BTN_W, $CTRL_H)

Local $y4 = $y3 + 50
Local $idGenerate   = GUICtrlCreateButton("Generate", $LX, $y4, 100, 30)
Local $idChkOpen2   = GUICtrlCreateCheckbox("Open manifest", $LX + 120, $y4 + 5, 120, 20)
GUICtrlSetState($idChkOpen2, $GUI_UNCHECKED)

GUICtrlCreatePic("", 10, $y4 + 90, $WIN_W - 20, 2)

; === Section 3: Blank OBJ Creator ===
Local $y5Title = $y4 + 100
Local $idTitle3 = GUICtrlCreateLabel("Chasm OBJ Blank file creator (blank.obj)", $LX, $y5Title, $WIN_W - 20, 24)
GUICtrlSetFont($idTitle3, 12, 800)
GUICtrlCreatePic("", 10, $y5Title + 30, $WIN_W - 20, 2)

Local $y5 = $y5Title + 40
GUICtrlCreateLabel("Width (px):",  $LX,       $y5,    80, $CTRL_H)
Local $idWidth   = GUICtrlCreateInput("64",   $LX + 90,  $y5,    60, $CTRL_H)
GUICtrlCreateLabel("Height (px):", $LX + 160, $y5,    80, $CTRL_H)
Local $idHeight  = GUICtrlCreateInput("128",  $LX + 250, $y5,    60, $CTRL_H)
GUICtrlCreateLabel("Origin (px):", $LX + 320, $y5,    80, $CTRL_H)
Local $idOrigin  = GUICtrlCreateInput("32",   $LX + 410, $y5,    60, $CTRL_H)
GUICtrlCreateLabel("Frames (#):",  $LX + 480, $y5,    80, $CTRL_H)
Local $idFrames  = GUICtrlCreateInput("1",    $LX + 570, $y5,    60, $CTRL_H)
GUICtrlCreateLabel("Palette idx:", $LX,       $y5 + 40,80, $CTRL_H)
Local $idPalette = GUICtrlCreateInput("176",  $LX + 90,  $y5 + 40,60, $CTRL_H)

Local $y6 = $y5 + 80
Local $idDummy = GUICtrlCreateButton("Create Blank", $LX, $y6, 120, 30)

GUICtrlCreatePic("", 10, $y6 + 50, $WIN_W - 20, 2)

; === Section 4: Create OBJ from Manifest ===
Local $y7Title = $y6 + 100
Local $idTitle4 = GUICtrlCreateLabel("Create new OBJ file using manifest", $LX, $y7Title, $WIN_W - 20, 24)
GUICtrlSetFont($idTitle4, 12, 800)
GUICtrlCreatePic("", 10, $y7Title + 30, $WIN_W - 20, 2)

Local $y7 = $y7Title + 40
GUICtrlCreateLabel("Manifest TXT File:", $LX, $y7, $LABEL_W, $CTRL_H)
Local $idMan3Input  = GUICtrlCreateInput("", $LX + $LABEL_W, $y7, $INPUT_W, $CTRL_H)
Local $idMan3Browse = GUICtrlCreateButton("Browse...", $LX + $LABEL_W + $INPUT_W + $GAP, $y7, $BTN_W, $CTRL_H)

Local $y8 = $y7 + 50
GUICtrlCreateLabel("New OBJ Name:", $LX, $y8, $LABEL_W, $CTRL_H)
Local $idNewObj    = GUICtrlCreateInput("new", $LX + $LABEL_W, $y8, $INPUT_W, $CTRL_H)
Local $idCreateObj = GUICtrlCreateButton("Create OBJ", $LX, $y8 + 40, 120, 30)

; Show everything
GUISetState(@SW_SHOW)

; === Main loop ===
While True
    Switch GUIGetMsg()

        Case $GUI_EVENT_CLOSE
            ExitLoop

        ; --- Section 1: Export ---
        Case $idBrowse
            Local $sFile = FileOpenDialog("Select Sprite OBJ file", @ScriptDir, "OBJ Files (*.obj)")
            If @error = 0 And $sFile <> "" Then
                GUICtrlSetData($idInput, $sFile)
            EndIf

        Case $idExport
            ; clear old
            GUICtrlSetData($idManifestEd, "")
            ; validate
            Local $sObj = GUICtrlRead($idInput)
            If $sObj = "" Or Not FileExists($sObj) Then
                MsgBox($MB_ICONERROR, "Error", "Please select a valid .OBJ file.")
                ContinueLoop
            EndIf
            ; run
            Local $iRet = RunWait('"' & $OBJTOOL & '" -export "' & $sObj & '"', @ScriptDir)
            If $iRet <> 0 Then
                MsgBox($MB_ICONERROR, "Error", "objtool.exe returned error code " & $iRet)
                ContinueLoop
            EndIf
            ; load manifest
            Local $sName     = StringRegExpReplace($sObj, '^.*\\', '')
            Local $sBase     = StringRegExpReplace($sName, '(?i)\.obj$', '')
            Local $sDir      = StringLeft($sObj, StringLen($sObj) - StringLen($sName))
            Local $sManifest = $sDir & $sBase & ".txt"
            If FileExists($sManifest) Then
                Local $raw = FileRead($sManifest)
                Local $rel = StringReplace($raw, $sDir, "")
                GUICtrlSetState($idManifestEd, $GUI_ENABLE)
                GUICtrlSetData($idManifestEd, $rel)
                GUICtrlSetState($idManifestEd, $GUI_DISABLE)
            EndIf
            ; optionally open
            If GUICtrlRead($idChkFolder) = $GUI_CHECKED Then ShellExecute($sDir & $sBase)
            If GUICtrlRead($idChkManifest) = $GUI_CHECKED Then ShellExecute($sManifest)

        ; --- Section 2: Manifest Creator ---
        Case $idMan2Browse
            Local $sFull = FileSelectFolder("Select PNG folder", @ScriptDir)
            If @error = 0 And $sFull <> "" Then
                GUICtrlSetData($idMan2Input, $sFull)
            EndIf

        Case $idGenerate
            GUICtrlSetData($idManifestEd, "")
            Local $sFull = GUICtrlRead($idMan2Input)
            If $sFull = "" Or Not FileExists($sFull) Then
                MsgBox($MB_ICONERROR, "Error", "Please select a valid PNG folder.")
                ContinueLoop
            EndIf
            Local $base = StringRegExpReplace($sFull, '^.*\\', '')
            Sleep(200)
            Local $iM = RunWait('"' & $OBJTOOL & '" -manifest "' & $base & '"', @ScriptDir)
            If $iM <> 0 Then
                MsgBox($MB_ICONERROR, "Error", "objtool.exe returned error code " & $iM)
                ContinueLoop
            EndIf
            Local $man2 = @ScriptDir & "\" & $base & ".txt"
            If Not FileExists($man2) Then
                MsgBox($MB_ICONWARNING, "Notice", "Manifest not found:" & @CRLF & $man2)
                ContinueLoop
            EndIf
            GUICtrlSetState($idManifestEd, $GUI_ENABLE)
            GUICtrlSetData($idManifestEd, FileRead($man2))
            GUICtrlSetState($idManifestEd, $GUI_DISABLE)
            If GUICtrlRead($idChkOpen2) = $GUI_CHECKED Then ShellExecute($man2)

        ; --- Section 3: Blank OBJ Creator ---
        Case $idDummy
            Local $w = Number(GUICtrlRead($idWidth))
            Local $h = Number(GUICtrlRead($idHeight))
            Local $o = GUICtrlRead($idOrigin)
            If $o = "" Then
                $o = Int($w / 2)
            EndIf
            Local $f = Number(GUICtrlRead($idFrames))
            Local $p = Number(GUICtrlRead($idPalette))
            ; warnings
            If $w > 64 Then
                MsgBox($MB_ICONWARNING, "Warning", "Width >64 px may not be supported.")
            EndIf
            If $h > 128 Then
                MsgBox($MB_ICONWARNING, "Warning", "Height >128 px may not be supported.")
            EndIf
            If $f > 15 Then
                MsgBox($MB_ICONWARNING, "Warning", "Frames >15 may not be supported.")
            EndIf
            ; run dummy
            Local $cmd = '"' & $OBJTOOL & '" -dummy ' & $w & ' ' & $h & ' ' & $o & ' ' & $f & ' ' & $p
            Local $r = RunWait($cmd, @ScriptDir)
            If $r <> 0 Then
                MsgBox($MB_ICONERROR, "Error", "objtool.exe returned error code " & $r)
                ContinueLoop
            EndIf
            ; open blank.obj
            Local $blank = @ScriptDir & "\blank.obj"
            If FileExists($blank) Then
                ShellExecute($OBJVIEWER, '"' & $blank & '"')
            EndIf

        ; --- Section 4: Create OBJ from Manifest ---
        Case $idMan3Browse
            Local $sM = FileOpenDialog("Select manifest txt", @ScriptDir, "Text Files (*.txt)")
            If @error = 0 And $sM <> "" Then
                GUICtrlSetData($idMan3Input, $sM)
                ; show it
                Local $data = FileRead($sM)
                GUICtrlSetState($idManifestEd, $GUI_ENABLE)
                GUICtrlSetData($idManifestEd, $data)
                GUICtrlSetState($idManifestEd, $GUI_DISABLE)
            EndIf

        Case $idCreateObj
            Local $sM  = GUICtrlRead($idMan3Input)
            Local $new = GUICtrlRead($idNewObj)
            If $sM = "" Or Not FileExists($sM) Then
                MsgBox($MB_ICONERROR, "Error", "Select a valid manifest txt.")
                ContinueLoop
            EndIf
            If $new = "" Then
                MsgBox($MB_ICONERROR, "Error", "Enter a name for the new OBJ (no extension).")
                ContinueLoop
            EndIf
            ; force .obj
            If StringLower(StringRight($new,4)) <> ".obj" Then
                $new &= ".obj"
            EndIf
            ; run create
            Local $c = RunWait('"' & $OBJTOOL & '" -create "' & $sM & '" "' & $new & '"', @ScriptDir)
            If $c <> 0 Then
                MsgBox($MB_ICONERROR, "Error", "objtool.exe returned error code " & $c)
                ContinueLoop
            EndIf
            ; open in viewer
            Local $pathNew = @ScriptDir & "\" & $new
            If FileExists($pathNew) Then
                ShellExecute($OBJVIEWER, '"' & $pathNew & '"')
            EndIf

    EndSwitch
WEnd

Exit
