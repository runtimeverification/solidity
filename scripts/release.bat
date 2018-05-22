@ECHO OFF

REM ---------------------------------------------------------------------------
REM Batch file for implementing release flow for solidity for Windows.
REM
REM The documentation for solidity is hosted at:
REM
REM     https://solidity.readthedocs.org
REM
REM ---------------------------------------------------------------------------
REM This file is part of solidity.
REM
REM solidity is free software: you can redistribute it and/or modify
REM it under the terms of the GNU General Public License as published by
REM the Free Software Foundation, either version 3 of the License, or
REM (at your option) any later version.
REM
REM solidity is distributed in the hope that it will be useful,
REM but WITHOUT ANY WARRANTY; without even the implied warranty of
REM MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
REM GNU General Public License for more details.
REM
REM You should have received a copy of the GNU General Public License
REM along with solidity.  If not, see <http://www.gnu.org/licenses/>
REM
REM Copyright (c) 2016 solidity contributors.
REM ---------------------------------------------------------------------------

set CONFIGURATION=%1

7z a solidity-windows.zip ^
    .\build\solc\%CONFIGURATION%\isolc.exe .\build\test\%CONFIGURATION%\soltest.exe ^
    "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\redist\x86\Microsoft.VC140.CRT\msvc*.dll"
