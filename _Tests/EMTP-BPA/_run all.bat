@echo off

setlocal enabledelayedexpansion
set num_cases=7
FOR /L %%A IN (1, 1, %num_cases%) do (
set n=0000%%A
set name=case!n:~-4!
call "_bin\ConsoleEMTP-BPA.exe" "!name!\test.dat"
copy /Y "!name!\*.out" "!name!\!name!_result.txt" >NUL
)


rem call "_bin\ConsoleEMTP-BPA.exe" "case0001\test.dat"
rem copy /Y "case0001\*.out" "case0001\case0001_result.txt" >NUL
rem call "_bin\ConsoleEMTP-BPA.exe" "case0002\test.dat"
rem copy /Y "case0002\*.out" "case0002\case0002_result.txt" >NUL


rem pause 