@echo off
echo =========================================
echo   Building Asm Validator (Standalone)
echo =========================================
echo.

REM Try g++ first (MinGW), then cl (MSVC)
where g++ >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Compiler: g++ (MinGW)
    g++ -std=c++17 -o asm_validator.exe standalone/AsmValidatorStandalone.cpp
    if %ERRORLEVEL% NEQ 0 (
        echo BUILD FAILED!
        pause
        exit /b 1
    )
    goto :run
)

where cl >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Compiler: cl (MSVC)
    cl /EHsc /std:c++17 /Fe:asm_validator.exe standalone/AsmValidatorStandalone.cpp
    if %ERRORLEVEL% NEQ 0 (
        echo BUILD FAILED!
        pause
        exit /b 1
    )
    goto :run
)

echo ERROR: No C++ compiler found!
echo Install one of:
echo   - MinGW-w64 (g++)
echo   - Visual Studio Build Tools (cl)
pause
exit /b 1

:run
echo.
echo BUILD SUCCESSFUL!
echo.
echo =========================================
echo   Running on BUGGY test files
echo =========================================
echo.
asm_validator.exe tests/buggy_1_missing_clobber.c tests/buggy_2_invalid_register.c tests/buggy_3_stack_misalign.c tests/buggy_4_callee_saved.c

echo.
echo =========================================
echo   Running on CORRECT test files
echo =========================================
echo.
asm_validator.exe tests/correct_1_basic_add.c tests/correct_2_syscall.c tests/correct_3_full_constraints.c

echo.
pause
