@echo off
rem What a press of Start does in console mode, measured without a window: five
rem trials, each one a single press, at a different place in the session. The log
rem of the states (--modes) is printed beside it, under Sega's names, so one sees
rem what was decided AND what the game made of it.
rem
rem Title screen = mode ADV, sub-state 4 or 5. See docs\fv2-states.md.
rem Allow two to three minutes in all.
setlocal
set ROM=C:\Users\frede\Documents\SEGA Model 3\ROMs\fvipers2.zip
set RUN=build\bin\vf3run.exe
set SCRIPT=tools\scripts\fv2-console-start.txt
cd /d "%~dp0"
if not exist "%RUN%" (
  echo %RUN% is missing: run build-tools.cmd first.
  pause
  exit /b 1
)

echo ######## control: no press at all
"%RUN%" "%ROM%" --frames 1700 --modes

echo.
echo ######## 1: Start during the SEGA logo (frame 1100)
"%RUN%" "%ROM%" --frames 1500 --modes --console-start 1100

echo.
echo ######## 2: Start on the title screen (frame 1255) -- the list, nothing written
"%RUN%" "%ROM%" --frames 1500 --modes --console-start 1255

echo.
echo ######## 3: Start during the demonstration (frame 1450)
"%RUN%" "%ROM%" --frames 1800 --modes --console-start 1450

echo.
echo ######## 4: Start on the select screen (frame 2100)
"%RUN%" "%ROM%" --script "%SCRIPT%" --modes --console-mode arcade --console-at 1960 --console-start 2100 --frames 2600

echo.
echo ######## 5: Start mid-fight (frame 3100)
"%RUN%" "%ROM%" --script "%SCRIPT%" --modes --console-mode arcade --console-at 1960 --console-start 3100 --frames 3400

echo.
echo ---- finished (code %ERRORLEVEL%) ----
pause
