@echo off
setlocal

:: Set variables
set RELEASE_DIR=Release
set BIN_DIR=%RELEASE_DIR%\bin
set CONFIG_DIR=%RELEASE_DIR%\config
set LOGS_DIR=%RELEASE_DIR%\logs
set BUILD_DIR=build

:: Create directory structure
if exist %RELEASE_DIR% rd /s /q %RELEASE_DIR%
mkdir %BIN_DIR%
mkdir %CONFIG_DIR%
mkdir %LOGS_DIR%

:: Copy executable
copy %BUILD_DIR%\bin\Release\myotronics_driver.exe %BIN_DIR%\

:: Create detailed config file
echo # Myotronics Driver Configuration > %CONFIG_DIR%\settings.conf
echo. >> %CONFIG_DIR%\settings.conf
echo # Serial Port Configuration >> %CONFIG_DIR%\settings.conf
echo port=COM1 >> %CONFIG_DIR%\settings.conf
echo baud_rate=115200 >> %CONFIG_DIR%\settings.conf
echo data_bits=8 >> %CONFIG_DIR%\settings.conf
echo stop_bits=1 >> %CONFIG_DIR%\settings.conf
echo parity=none >> %CONFIG_DIR%\settings.conf
echo. >> %CONFIG_DIR%\settings.conf
echo # Debug Settings >> %CONFIG_DIR%\settings.conf
echo debug=false >> %CONFIG_DIR%\settings.conf
echo log_level=INFO >> %CONFIG_DIR%\settings.conf
echo log_file=../logs/myotronics.log >> %CONFIG_DIR%\settings.conf
echo. >> %CONFIG_DIR%\settings.conf
echo # HTTP Server Settings >> %CONFIG_DIR%\settings.conf
echo http_port=8080 >> %CONFIG_DIR%\settings.conf
echo http_host=0.0.0.0 >> %CONFIG_DIR%\settings.conf

:: Create detailed readme
echo Myotronics Driver > %RELEASE_DIR%\readme.txt
echo ================== >> %RELEASE_DIR%\readme.txt
echo. >> %RELEASE_DIR%\readme.txt
echo Version: 1.0.0 >> %RELEASE_DIR%\readme.txt
echo. >> %RELEASE_DIR%\readme.txt
echo Directory Structure: >> %RELEASE_DIR%\readme.txt
echo - bin/: Contains the main executable >> %RELEASE_DIR%\readme.txt
echo - config/: Configuration files >> %RELEASE_DIR%\readme.txt
echo - logs/: Log files will be stored here >> %RELEASE_DIR%\readme.txt
echo. >> %RELEASE_DIR%\readme.txt
echo Configuration: >> %RELEASE_DIR%\readme.txt
echo Edit settings.conf in the config directory to modify program settings. >> %RELEASE_DIR%\readme.txt
echo. >> %RELEASE_DIR%\readme.txt
echo For support contact: support@myotronics.com >> %RELEASE_DIR%\readme.txt

:: Create .gitignore if it doesn't exist
if not exist .gitignore (
    echo # Build directories > .gitignore
    echo build/ >> .gitignore
    echo Release/ >> .gitignore
    echo. >> .gitignore
    echo # Log files >> .gitignore
    echo logs/ >> .gitignore
    echo *.log >> .gitignore
    echo. >> .gitignore
    echo # IDE files >> .gitignore
    echo .vs/ >> .gitignore
    echo *.suo >> .gitignore
    echo *.user >> .gitignore
)

echo Package created successfully in %RELEASE_DIR%