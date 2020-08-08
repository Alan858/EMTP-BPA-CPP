@echo off

call "_bin\ConsoleEMTP-BPA.exe" "case0001\test.dat"
copy /Y "case0001\*.csv" "case0001\case0001_result.txt" >NUL

call "_bin\ConsoleEMTP-BPA.exe" "case0002\test.dat"
copy /Y "case0002\*.csv" "case0002\case0002_result.txt" >NUL






rem pause 