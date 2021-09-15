@echo off
for /r %%i in (*.frag) do F:\vulkan_shooter\sdk\1.2.176.1\Bin\glslc.exe "%%i" -o %%~ni.fspv
for /r %%i in (*.vert) do F:\vulkan_shooter\sdk\1.2.176.1\Bin\glslc.exe "%%i" -o %%~ni.vspv
pause