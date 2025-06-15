; AutoIt3 Script: GUI wrapper for celtool.exe and celviewer.exe
; Save as celtool_gui.au3 and compile with AutoIt3

#include <GUIConstantsEx.au3>
#include <WindowsConstants.au3>
#include <File.au3> ; For _PathSplit and file-dialog constants
Opt("MustDeclareVars", 1)

; Edit control styles for the path field
Global Const $ES_READONLY    = 0x0800
Global Const $ES_AUTOVSCROLL = 0x0040
Global Const $ES_MULTILINE   = 0x0004
Global Const $ES_AUTOHSCROLL = 0x0080

; GUI state variables
Global $mode = 1 ; 1 = Export PNG, 2 = Create CEL
Global $filepath = ""

; Create main window
Local $hGUI = GUICreate("Chasm CEL Toolkit GUI v1.0.1 by SMR9000", 450, 220)
GUISetIcon("", 1)

; Mode selection radio buttons
Local $rbExport = GUICtrlCreateRadio("Export PNG", 20, 20, 120, 20)
Local $rbCreate = GUICtrlCreateRadio("Create CEL", 160, 20, 120, 20)
GUICtrlSetState($rbExport, $GUI_CHECKED)

; File selection field (read-only multi-line edit)
Local $btnBrowse = GUICtrlCreateButton("Browse...", 20, 60, 80, 25)
Local $lblFile = GUICtrlCreateEdit("No file selected", 120, 60, 300, 40, _
    BitOR($ES_READONLY, $ES_AUTOVSCROLL, $ES_AUTOHSCROLL, $ES_MULTILINE, $WS_VSCROLL, $WS_HSCROLL))

; Dithering options (exclusive)
Local $chkDiff    = GUICtrlCreateCheckbox("Diffusion", 20, 120, 100, 20)
Local $chkPattern = GUICtrlCreateCheckbox("Pattern", 140, 120, 100, 20)
Local $chkNoise   = GUICtrlCreateCheckbox("Noise",   260, 120, 100, 20)
GUICtrlSetState($chkDiff,    $GUI_DISABLE)
GUICtrlSetState($chkPattern, $GUI_DISABLE)
GUICtrlSetState($chkNoise,   $GUI_DISABLE)

; Execute and Exit
Local $btnExec = GUICtrlCreateButton("Execute", 140, 180, 80, 30)
Local $btnExit = GUICtrlCreateButton("Exit",    260, 180, 80, 30)

GUISetState(@SW_SHOW)

; Main loop
While True
    Switch GUIGetMsg()
        Case $GUI_EVENT_CLOSE, $btnExit
            ExitLoop

        ; Mode switch
        Case $rbExport
            $mode = 1
            GUICtrlSetState($chkDiff,    $GUI_DISABLE)
            GUICtrlSetState($chkPattern, $GUI_DISABLE)
            GUICtrlSetState($chkNoise,   $GUI_DISABLE)

        Case $rbCreate
            $mode = 2
            GUICtrlSetState($chkDiff,    $GUI_ENABLE)
            GUICtrlSetState($chkPattern, $GUI_ENABLE)
            GUICtrlSetState($chkNoise,   $GUI_ENABLE)

        ; Browse for file
        Case $btnBrowse
            Local $file
            If $mode = 1 Then
                $file = FileOpenDialog("Select CEL file...", @ScriptDir, "Autodesk CEL (*.cel)", $FD_FILEMUSTEXIST)
            Else
                $file = FileOpenDialog("Select PNG file...", @ScriptDir, "PNG Image (*.png)", $FD_FILEMUSTEXIST)
            EndIf
            If @error = 0 Then
                $filepath = $file
                GUICtrlSetData($lblFile, $filepath)
            EndIf

        ; Exclusive checkbox logic
        Case $chkDiff
            If GUICtrlRead($chkDiff) = $GUI_CHECKED Then
                GUICtrlSetState($chkPattern, $GUI_UNCHECKED)
                GUICtrlSetState($chkNoise,   $GUI_UNCHECKED)
            EndIf
        Case $chkPattern
            If GUICtrlRead($chkPattern) = $GUI_CHECKED Then
                GUICtrlSetState($chkDiff,  $GUI_UNCHECKED)
                GUICtrlSetState($chkNoise, $GUI_UNCHECKED)
            EndIf
        Case $chkNoise
            If GUICtrlRead($chkNoise) = $GUI_CHECKED Then
                GUICtrlSetState($chkDiff,    $GUI_UNCHECKED)
                GUICtrlSetState($chkPattern, $GUI_UNCHECKED)
            EndIf

        ; Execute command
        Case $btnExec
            If $filepath = "" Then
                MsgBox(16, "Error", "Please select a file first.")
                ContinueLoop
            EndIf

            If $mode = 1 Then
                ; Export PNG
                Local $cmd = '"' & @ScriptDir & '\\celtool.exe" -export "' & $filepath & '"'
                RunWait($cmd, @ScriptDir, @SW_HIDE)
                MsgBox(64, "Done", "Export complete! Check the output PNG and alpha mask files.")
            Else
                ; Create CEL with exactly one optional flag
                Local $flags = ""
                If GUICtrlRead($chkDiff) = $GUI_CHECKED Then
                    $flags = " -diffusion"
                ElseIf GUICtrlRead($chkPattern) = $GUI_CHECKED Then
                    $flags = " -pattern"
                ElseIf GUICtrlRead($chkNoise) = $GUI_CHECKED Then
                    $flags = " -noise"
                EndIf

                Local $cmd = '"' & @ScriptDir & '\\celtool.exe" -convert "' & $filepath & '"' & $flags
                RunWait($cmd, @ScriptDir, @SW_HIDE)

                ; Determine output .cel path
                Local $drive, $dir, $name, $ext
                _PathSplit($filepath, $drive, $dir, $name, $ext)
                Local $suffix = ""
                If GUICtrlRead($chkDiff) = $GUI_CHECKED Then
                    $suffix = "_diffusion"
                ElseIf GUICtrlRead($chkPattern) = $GUI_CHECKED Then
                    $suffix = "_pattern"
                ElseIf GUICtrlRead($chkNoise) = $GUI_CHECKED Then
                    $suffix = "_noise"
                EndIf
                Local $outCel = $drive & $dir & $name & $suffix & ".cel"

                MsgBox(64, "Done", "Conversion complete! Output: " & $outCel)
                Run('"' & @ScriptDir & '\\celviewer.exe" "' & $outCel & '"', @ScriptDir)
            EndIf
    EndSwitch
WEnd

Exit