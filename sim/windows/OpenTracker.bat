@echo off
rem Starts the OpenTracker simulator with the SD card folder next to this file.
rem Extra arguments are passed on, e.g.  OpenTracker.bat --midi-in KeyStep
"%~dp0opentracker-sim.exe" --sd "%~dp0sd-card" %*
if errorlevel 1 pause
