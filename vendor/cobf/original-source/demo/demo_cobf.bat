@echo off
echo shroud all the COBF sources!!
..\src\win32\release\cobf @demo_token.inv -x bb -v -o output -p ..\etc\pp_ger_msvc.bat @cobfiles.inv -i ..\src\include -i ..\src\cpp -c cobf.cpp
