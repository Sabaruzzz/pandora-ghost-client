@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++latest /EHsc /c "D:\pandoraclient\snowinternal\w_imgui_port\w_core_probe.cpp" /Fo"D:\pandoraclient\snowinternal\w_imgui_port\w_core_probe.obj"