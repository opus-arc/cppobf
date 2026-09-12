@echo off
rem Example preprocessor script for Visual C++ (german installation path)
rem please adapt to your local configuration!
echo %0 %1 %2
call "C:\Programme\Microsoft Visual Studio .NET 2003\Common7\Tools\VSVARS32.BAT"
rem use /E instead of /EP if you need the #line directive
cl /EP %1 >%2
