@echo off
setlocal

cd /d %~dp0

if not exist Converted (
    mkdir Converted
)

for %%f in (*.dds) do (
    texconv.exe -y -m 0 -o Converted "%%f"
)

pause