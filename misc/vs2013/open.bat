@echo off

:: set BJARKE_DEBUG_PATH to the absolute path of the build directory
pushd ..\..\..
set BJARKE_DEBUG_PATH=%CD%
IF %BJARKE_DEBUG_PATH:~-1%==\ SET BJARKE_DEBUG_PATH=%BJARKE_DEBUG_PATH:~0,-1%
popd

:: open the Visual C++ solution
Bachelor.sln
