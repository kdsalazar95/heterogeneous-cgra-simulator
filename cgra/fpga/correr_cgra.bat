@echo off
REM correr_cgra.bat - Doble clic. HLS de las variantes 1 y 2 + bitstream de cada una + ZIP. ~1-1.5 h.
cd /d "%~dp0"
echo ===== Carpeta: %cd%
if not exist run_hls.tcl ( echo ERROR: este .bat debe estar dentro de la carpeta fpga & pause & exit /b 1 )
call C:\AMDDesignTools\2026.1\Vitis\settings64.bat
if exist C:\AMDDesignTools\2026.1\Vivado\settings64.bat call C:\AMDDesignTools\2026.1\Vivado\settings64.bat
echo ===== [1/4] HLS variantes 1 y 2 (csim + sintesis + cosim) ... %time%
set VARIANTES=1 2
call vitis-run --mode hls --tcl run_hls.tcl > log_hls.txt 2>&1
findstr /C:"Estimated Fmax" /C:"AVISO" /C:"PASA" /C:"FALLA" log_hls.txt
echo ===== [2/4] Bitstream variante 2 (~25 min) ... %time%
if exist proy_v2\sol\impl\ip ( call vivado -mode batch -nojournal -source build_bitstream.tcl -tclargs 2 > log_vivado_v2.txt 2>&1 )
echo ===== [3/4] Bitstream variante 1 (~25 min) ... %time%
if exist proy_v1\sol\impl\ip ( call vivado -mode batch -nojournal -source build_bitstream.tcl -tclargs 1 > log_vivado_v1.txt 2>&1 )
echo ===== [4/4] Empaquetando ... %time%
if not exist overlay mkdir overlay
powershell -NoProfile -Command "Compress-Archive -Force -Path reportes,overlay,log_*.txt -DestinationPath cgra_fpga_para_ronald.zip"
echo.
echo ===== LISTO: enviar a Ronald %cd%\cgra_fpga_para_ronald.zip
pause
