#include <GuiEdit.au3>
#include <ButtonConstants.au3>
#include <EditConstants.au3>
#include <FileConstants.au3>
#include <GUIConstantsEx.au3>
#include <StaticConstants.au3>
#include <WindowsConstants.au3>
#include <MsgBoxConstants.au3>
#include <ColorConstants.au3>
#include <WinAPIRes.au3>

;—————————————— Globals ——————————————
Global $sVersion         = "1.3"
Global $sCopyright       = "© SMR9000"
Global $sCompanyName     = "SMR9000"
Global $sProductName     = "Chasm CAR Texture Replacer GUI"
Global $sDescription     = "GUI Interface for carreplace.exe and carviewer.exe"
Global $sIconPath        = @ScriptDir & "\carreplace_gui.ico"

Global $defaultPalette   = @ScriptDir & "\chasmpalette.act"
Global $carReplaceExe    = @ScriptDir & "\carreplace.exe"
Global $carViewerExe     = @ScriptDir & "\carviewer.exe"

;—————————————— Koda GUI section ——————————————
$Form1 = GUICreate("Chasm CAR Texture Replacer v" & $sVersion & " - GUI Interface by SMR9000", 615, 440)

; GUI controls
$Label1   = GUICtrlCreateLabel("CAR Model File:", 20, 20, 90, 20)
$Input1   = GUICtrlCreateInput("", 120, 20, 400, 20)
$Button1  = GUICtrlCreateButton("Browse", 530, 20, 60, 20)

$Label2   = GUICtrlCreateLabel("PNG Texture File:", 20, 60, 90, 20)
$Input2   = GUICtrlCreateInput("", 120, 60, 400, 20)
$Button2  = GUICtrlCreateButton("Browse", 530, 60, 60, 20)

$Label3   = GUICtrlCreateLabel("Palette (ACT):", 20, 100, 90, 20)
$Input3   = GUICtrlCreateInput($defaultPalette, 120, 100, 400, 20)
$Button3  = GUICtrlCreateButton("Browse", 530, 100, 60, 20)
$Checkbox1= GUICtrlCreateCheckbox("Use default Chasm palette", 120, 130, 200, 20)
GUICtrlSetState(-1, $GUI_CHECKED)
GUICtrlSetState($Input3, $GUI_DISABLE)
GUICtrlSetState($Button3, $GUI_DISABLE)

$Label4   = GUICtrlCreateLabel("Output CAR File:", 20, 170, 90, 20)
$Input4   = GUICtrlCreateInput("", 120, 170, 400, 20)
$Button4  = GUICtrlCreateButton("Browse", 530, 170, 60, 20)

$Label5   = GUICtrlCreateLabel("Status:", 20, 210, 50, 20)
$Edit1    = GUICtrlCreateEdit("Ready." & @CRLF, 20, 230, 570, 150, BitOR($ES_READONLY, $ES_WANTRETURN, $WS_VSCROLL))
GUICtrlSetBkColor(-1, 0xF0F0F0)

$Label6   = GUICtrlCreateLabel("New texture must be same size as original; transparency uses value #040404 in palette.", 0, 385, 615, 20, $SS_CENTER)
GUICtrlSetColor($Label6, $COLOR_RED)

$Button5  = GUICtrlCreateButton("Replace Texture", 240, 405, 120, 30)
GUICtrlSetFont($Button5, 10, 600, 0, "Segoe UI")

$Button6  = GUICtrlCreateButton("Show original model", 380, 405, 150, 30)
GUICtrlSetFont($Button6, 10, 700, 0, "Segoe UI")

GUISetState(@SW_SHOW)

;—————————————— Main message loop ——————————————
While 1
    $nMsg = GUIGetMsg()
    Switch $nMsg
        Case $GUI_EVENT_CLOSE
            Exit

        Case $Button1
            Local $file = FileOpenDialog("Select CAR model file", @ScriptDir, "CAR Files (*.car)", $FD_FILEMUSTEXIST)
            If Not @error Then
                GUICtrlSetData($Input1, $file)
                If GUICtrlRead($Input4) = "" Then
                    GUICtrlSetData($Input4, StringTrimRight($file, 4) & "_modified.car")
                EndIf
            EndIf

        Case $Button2
            Local $file = FileOpenDialog("Select PNG texture file", @ScriptDir, "PNG Files (*.png)", $FD_FILEMUSTEXIST)
            If Not @error Then GUICtrlSetData($Input2, $file)

        Case $Button3
            Local $file = FileOpenDialog("Select ACT palette file", @ScriptDir, "ACT Files (*.act)", $FD_FILEMUSTEXIST)
            If Not @error Then GUICtrlSetData($Input3, $file)

        Case $Button4
            Local $file = FileSaveDialog("Save output CAR file", @ScriptDir, "CAR Files (*.car)", $FD_PATHMUSTEXIST)
            If Not @error Then GUICtrlSetData($Input4, $file)

        Case $Checkbox1
            If GUICtrlRead($Checkbox1) = $GUI_CHECKED Then
                GUICtrlSetData($Input3, $defaultPalette)
                GUICtrlSetState($Input3, $GUI_DISABLE)
                GUICtrlSetState($Button3, $GUI_DISABLE)
            Else
                GUICtrlSetState($Input3, $GUI_ENABLE)
                GUICtrlSetState($Button3, $GUI_ENABLE)
            EndIf

        Case $Button5
            ProcessFiles()

        Case $Button6
            Local $model = GUICtrlRead($Input1)
            If $model = "" Or Not FileExists($model) Then
                MsgBox($MB_ICONERROR, "Error", "Please select a valid CAR model file")
                GUICtrlSetData($Edit1, GUICtrlRead($Edit1) & "Error: No CAR file selected" & @CRLF)
            Else
                GUICtrlSetData($Edit1, GUICtrlRead($Edit1) & "Opening viewer on: " & $model & @CRLF)
                Run('"' & $carViewerExe & '" "' & $model & '"')
            EndIf
    EndSwitch
WEnd

;—————————————— Replace-texture function ——————————————
Func ProcessFiles()
    GUICtrlSetData($Edit1, "Processing..." & @CRLF)

    Local $carFile = GUICtrlRead($Input1)
    Local $pngFile = GUICtrlRead($Input2)
    Local $outputFile = GUICtrlRead($Input4)

    ; Validate inputs
    If $carFile = "" Or Not FileExists($carFile) Then
        MsgBox($MB_ICONERROR, "Error", "Please select a valid CAR model file")
        GUICtrlSetData($Edit1, GUICtrlRead($Edit1) & "Error: No CAR file selected" & @CRLF)
        Return
    EndIf
    If $pngFile = "" Or Not FileExists($pngFile) Then
        MsgBox($MB_ICONERROR, "Error", "Please select a valid PNG texture file")
        GUICtrlSetData($Edit1, GUICtrlRead($Edit1) & "Error: No PNG file selected" & @CRLF)
        Return
    EndIf

    Local $paletteFile
    If GUICtrlRead($Checkbox1) = $GUI_CHECKED Then
        $paletteFile = $defaultPalette
    Else
        $paletteFile = GUICtrlRead($Input3)
    EndIf
    If $paletteFile = "" Or Not FileExists($paletteFile) Then
        MsgBox($MB_ICONERROR, "Error", "Palette file not found: " & $paletteFile)
        GUICtrlSetData($Edit1, GUICtrlRead($Edit1) & "Error: Palette file missing" & @CRLF)
        Return
    EndIf

    If Not FileExists($carReplaceExe) Then
        MsgBox($MB_ICONERROR, "Error", "carreplace.exe not found")
        GUICtrlSetData($Edit1, GUICtrlRead($Edit1) & "Error: carreplace.exe missing" & @CRLF)
        Return
    EndIf

    If $outputFile = "" Then
        $outputFile = @ScriptDir & "\output.car"
        GUICtrlSetData($Input4, $outputFile)
    EndIf

    If FileExists($outputFile) Then
        Local $ans = MsgBox($MB_YESNO + $MB_ICONQUESTION, "Confirm", "Output file exists. Overwrite?")
        If $ans = $IDNO Then
            GUICtrlSetData($Edit1, GUICtrlRead($Edit1) & "Operation canceled by user" & @CRLF)
            Return
        EndIf
        FileDelete($outputFile)
        Local $i = 0
        While FileExists($outputFile) And $i < 50
            Sleep(100)
            $i += 1
        WEnd
    EndIf

    Local $cmd = '"' & $carReplaceExe & '" "' & $carFile & '" "' & $pngFile & '" -palette "' & $paletteFile & '" -output "' & $outputFile & '"'
    RunWait($cmd, @ScriptDir, @SW_HIDE)

    If FileExists($outputFile) Then
        Local $sz = FileGetSize($outputFile)
        If $sz > 0 Then
            GUICtrlSetData($Edit1, GUICtrlRead($Edit1) & @CRLF & "SUCCESS: Valid CAR created (" & $sz & " bytes)" & @CRLF)
            GUICtrlSetData($Edit1, GUICtrlRead($Edit1) & "Output: " & $outputFile & @CRLF)
        Else
            GUICtrlSetData($Edit1, GUICtrlRead($Edit1) & @CRLF & "WARNING: Empty file created!" & @CRLF)
        EndIf
    Else
        GUICtrlSetData($Edit1, GUICtrlRead($Edit1) & @CRLF & "ERROR: No output file created" & @CRLF)
    EndIf

    ControlSend($Form1, "", $Edit1, "^{END}")

    If FileExists($outputFile) Then
        GUICtrlSetData($Edit1, GUICtrlRead($Edit1) & "Opening viewer on: " & $outputFile & @CRLF)
        Run('"' & $carViewerExe & '" "' & $outputFile & '"')
    EndIf
EndFunc
