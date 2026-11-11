@echo off
set "JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-21.0.13.9-hotspot"
set "ANDROID_HOME=C:\Android"
set "PATH=%JAVA_HOME%\bin;%PATH%"
cd /d C:\Kodingan\fe9-port\poc
call C:\Tools\gradle\gradle-8.9\bin\gradle.bat %*
