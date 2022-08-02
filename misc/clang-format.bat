@echo off

set PATH=%cd%;%PATH%

:: -i means in-place (edit the file, don't write to stdout)
::set OPTIONS=-i -style=llvm
set OPTIONS=-i -style=file

pushd ..\code

echo Code: %cd%

pushd common
echo Processing %cd%
for %%f in (*.cpp) do echo %%f && clang-format %OPTIONS% %%f
for %%f in (*.h) do echo %%f && clang-format %OPTIONS% %%f
popd

pushd renderer
echo Processing %cd%
for %%f in (*.cpp) do echo %%f && clang-format %OPTIONS% %%f
for %%f in (r_*.h) do echo %%f && clang-format %OPTIONS% %%f
popd

pushd scene
echo Processing %cd%
for %%f in (*.cpp) do echo %%f && clang-format %OPTIONS% %%f
for %%f in (*.h) do echo %%f && clang-format %OPTIONS% %%f
popd

pushd shaders
echo Processing %cd%
for %%f in (*.hlsl) do echo %%f && clang-format %OPTIONS% %%f
for %%f in (*.hlsli) do echo %%f && clang-format %OPTIONS% %%f
popd

pushd tools
pushd hemisphere
echo Processing %cd%
for %%f in (*.cpp) do echo %%f && clang-format %OPTIONS% %%f
for %%f in (*.h) do echo %%f && clang-format %OPTIONS% %%f
popd
pushd mesh_baker
echo Processing %cd%
for %%f in (*.cpp) do echo %%f && clang-format %OPTIONS% %%f
for %%f in (*.h) do echo %%f && clang-format %OPTIONS% %%f
popd
popd

pushd win32
echo Processing %cd%
for %%f in (*.cpp) do echo %%f && clang-format %OPTIONS% %%f
for %%f in (*.h) do echo %%f && clang-format %OPTIONS% %%f
popd

popd
pause

::for %%f in ( ^
::	"\\sources\folder1\" ^
::	"\\sources\folder2\" ^
::	) do echo %%f && clang-format %OPTIONS% %%f
