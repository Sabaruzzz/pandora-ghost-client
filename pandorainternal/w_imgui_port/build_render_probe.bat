@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /std:c++latest /EHsc /c "D:\pandoraclient\snowinternal\w_imgui_port\render\render.cc" /Fo"D:\pandoraclient\snowinternal\w_imgui_port\render_probe.obj"
