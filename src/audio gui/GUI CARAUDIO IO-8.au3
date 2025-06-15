#include <GUIConstantsEx.au3>
#include <EditConstants.au3>
#include <FileConstants.au3>
#include <MsgBoxConstants.au3>
#include <WindowsConstants.au3>
#include <StaticConstants.au3>
#include <ColorConstants.au3>

; Configuration
Global $caraudioExe  = @ScriptDir & "\caraudio-io.exe"
Global $carViewerExe = @ScriptDir & "\carviewer.exe"

; Create main window
Local $hGUI = GUICreate("Chasm CAR Audio Replacement Tool v1.0.0 by SMR9000", 650, 650)

; — CAR Model selector —
GUICtrlCreateLabel("CAR Model File:", 20, 20, 100, 20)
Global $idInputCar   = GUICtrlCreateInput("", 130, 20, 400, 20)
Global $idBtnBrowse  = GUICtrlCreateButton("Browse...", 540, 20, 80, 20)

; — Output prefix (for extract) —
GUICtrlCreateLabel("Output Prefix:", 20, 60, 100, 20)
Global $idPrefix     = GUICtrlCreateInput("", 130, 60, 200, 20)

; — Export Format (mutually exclusive) —
GUICtrlCreateLabel("Export Format:", 20, 100, 100, 20)
Global $idChkRaw     = GUICtrlCreateCheckbox("RAW", 130, 100, 60, 20)
Global $idChkWav     = GUICtrlCreateCheckbox("WAV", 200, 100, 60, 20)
GUICtrlSetState($idChkWav, $GUI_CHECKED)    ; WAV by default

; — Primary action buttons —
Global $idBtnExtract = GUICtrlCreateButton("Extract Audio",     130, 140, 120, 30)
Global $idBtnShow    = GUICtrlCreateButton("Show Model",        270, 140, 120, 30)
Global $idBtnFolder  = GUICtrlCreateButton("Open Output Folder",410, 140, 180, 30)

; — Bank replacement section —
GUICtrlCreateGroup("Replace Banks (0–6)", 10, 190, 630, 260)
Global $aBankInputs[7], $aBankButtons[7]
For $i = 0 To 6
    Local $y = 220 + $i * 30
    GUICtrlCreateLabel("Bank " & $i & ":", 20, $y, 60, 20)
    $aBankInputs[$i]  = GUICtrlCreateInput("", 100, $y, 400, 20)
    $aBankButtons[$i] = GUICtrlCreateButton("Browse", 520, $y, 80, 20)
Next

; — Replace Audio button —
Global $idBtnReplace = GUICtrlCreateButton("Replace Audio", 270, 460, 120, 30)

; — Usage display —
GUICtrlCreateLabel("Usage:", 20, 500, 60, 20)
Global $idUsage = GUICtrlCreateEdit("", 80, 500, 560, 100, _
    BitOR($ES_READONLY, $WS_VSCROLL, $ES_AUTOVSCROLL))
GUICtrlSetBkColor($idUsage, 0xF0F0F0)

; — Important warning label —
Global $idWarn = GUICtrlCreateLabel( _
    "IMPORTANT: New sound files must be same length as original, saved as 8-bit @ 11025 Hz WAV or PCM RAW", _
    0, 620, 650, 20, $SS_CENTER)
GUICtrlSetColor($idWarn, $COLOR_RED)

; Show GUI
GUISetState(@SW_SHOW)

; Initial usage
_UpdateUsage()

; Main event loop
While True
    Local $nMsg = GUIGetMsg()
    Local $i

    ; Handle bank Browse buttons
    For $i = 0 To 6
        If $nMsg = $aBankButtons[$i] Then
            _BrowseBank($i)
            _UpdateUsage()
            ContinueLoop
        EndIf
    Next

    Switch $nMsg
        Case $GUI_EVENT_CLOSE
            Exit

        Case $idBtnBrowse
            Local $file = FileOpenDialog("Select .car file", @ScriptDir, "CAR Files (*.car)", $FD_FILEMUSTEXIST)
            If Not @error Then
                GUICtrlSetData($idInputCar, $file)
                ; auto-fill prefix
                Local $p = StringInStr($file, "\", 0, -1)
                Local $b = $p ? StringMid($file, $p + 1) : $file
                If StringLower(StringRight($b, 4)) = ".car" Then $b = StringTrimRight($b, 4)
                GUICtrlSetData($idPrefix, $b)
                _UpdateUsage()
            EndIf

        Case $idChkRaw
            GUICtrlSetState($idChkRaw, $GUI_CHECKED)
            GUICtrlSetState($idChkWav, $GUI_UNCHECKED)
            _UpdateUsage()

        Case $idChkWav
            GUICtrlSetState($idChkWav, $GUI_CHECKED)
            GUICtrlSetState($idChkRaw, $GUI_UNCHECKED)
            _UpdateUsage()

        Case $idBtnExtract
            Local $in = GUICtrlRead($idInputCar), $pre = GUICtrlRead($idPrefix)
            If $in = "" Or Not FileExists($in) Then MsgBox($MB_ICONERROR, "Error", "Select a .car file.") : ContinueLoop
            If $pre = "" Then MsgBox($MB_ICONERROR, "Error", "Specify prefix.") : ContinueLoop
            Local $fmt = (GUICtrlRead($idChkWav) = $GUI_CHECKED) ? "-wav" : "-raw"
            Local $d   = StringLeft($in, StringInStr($in, "\", 0, -1) - 1)
            Local $cmd = '"' & $caraudioExe & '" -export "' & $in & '" "' & $pre & '" ' & $fmt
            Run($cmd, $d, @SW_SHOW)
            _UpdateUsage()

        Case $idBtnShow
            Local $in = GUICtrlRead($idInputCar)
            If $in = "" Or Not FileExists($in) Then
                MsgBox($MB_ICONERROR, "Error", "No model selected.")
            Else
                Run('"' & $carViewerExe & '" "' & $in & '"')
            EndIf

        Case $idBtnFolder
            Local $in = GUICtrlRead($idInputCar)
            If $in = "" Or Not FileExists($in) Then
                MsgBox($MB_ICONERROR, "Error", "Select a .car first.")
            Else
                Local $d = StringLeft($in, StringInStr($in, "\", 0, -1) - 1)
                Run('explorer.exe "' & $d & '"')
            EndIf

        Case $idBtnReplace
            Local $inCar = GUICtrlRead($idInputCar)
            If $inCar = "" Or Not FileExists($inCar) Then MsgBox($MB_ICONERROR, "Error", "Load a .car first.") : ContinueLoop
            Local $p  = StringInStr($inCar, "\", 0, -1)
            Local $bs = $p ? StringMid($inCar, $p + 1) : $inCar
            If StringLower(StringRight($bs, 4)) = ".car" Then $bs = StringTrimRight($bs, 4)
            Local $outCar = StringLeft($inCar, $p) & $bs & "_newaudio.car"
            Local $cmd = '"' & $caraudioExe & '" -import "' & $inCar & '" "' & $outCar & '"'
            For $i = 0 To 6
                Local $f = GUICtrlRead($aBankInputs[$i])
                If $f <> "" Then $cmd &= ' "' & $f & '"'
            Next
            Local $w = StringLeft($inCar, $p - 1)
            RunWait($cmd, $w, @SW_SHOW)
            If FileExists($outCar) Then Run('"' & $carViewerExe & '" "' & $outCar & '"')
            _UpdateUsage()
    EndSwitch
WEnd

Func _BrowseBank($i)
    Local $file = FileOpenDialog("Select RAW/WAV for Bank " & $i, @ScriptDir, "Audio Files (*.raw;*.wav)", $FD_FILEMUSTEXIST)
    If Not @error Then GUICtrlSetData($aBankInputs[$i], $file)
EndFunc

Func _UpdateUsage()
    Local $in, $pre, $fmt, $imp, $exp, $bankArgs, $i, $f
    $in       = GUICtrlRead($idInputCar)
    $pre      = GUICtrlRead($idPrefix)
    $fmt      = (GUICtrlRead($idChkWav) = $GUI_CHECKED) ? "-wav" : "-raw"
    $imp      = 'caraudio-io.exe -import ' & ( $in   ? '"' & $in   & '"' : '<input.car>' )
    $exp      = 'caraudio-io.exe -export ' & ( $in   ? '"' & $in   & '"' : '<input.car>' ) & ' ' & _
                ( $pre  ? '"' & $pre  & '"' : '<prefix>' ) & ' ' & $fmt
    $bankArgs = ""
    For $i = 0 To 6
        $f = GUICtrlRead($aBankInputs[$i])
        If $f <> "" Then $bankArgs &= ' "' & $f & '"'
    Next
    GUICtrlSetData($idUsage, $imp & $bankArgs & @CRLF & $exp)
EndFunc
