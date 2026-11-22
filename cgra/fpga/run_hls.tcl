# run_hls.tcl - C-sim, síntesis, cosimulación y export del IP cgra_e4 (E4 de la CGRA).
#
# Uso, desde cgra/fpga (Vitis 2024.2 o posterior):
#     vitis-run --mode hls --tcl run_hls.tcl
# Variables de entorno opcionales:
#     VARIANTES="1 2"   variantes a correr (default: las dos)
#     COSIM=0           salta la cosimulación
#
# Por variante crea proy_v<k>/ y copia los reportes a reportes/v<k>/.
# El testbench compara contra la CPU el caso tb_datos/reduccion_4x4.

set sm_dir   [pwd]
set part     xck26-sfvc784-2LV-c
set periodo  10   ;# ns -> 100 MHz, el mismo pl_clk0 del diseño de Vivado
set variantes [expr {[info exists ::env(VARIANTES)] ? $::env(VARIANTES) : "1 2"}]
set do_cosim  [expr {[info exists ::env(COSIM)]     ? $::env(COSIM)     : 1}]
set datos     $sm_dir/tb_datos/reduccion_4x4

file mkdir $sm_dir/reportes
foreach v $variantes {
    set proy proy_v$v
    puts "=================== $proy ==================="
    open_project -reset $proy
    set_top cgra_e4
    add_files     $sm_dir/cgra_hls.cpp -cflags "-DVARIANTE=$v"
    add_files     $sm_dir/cgra_hls.h
    add_files -tb $sm_dir/cgra_tb.cpp  -cflags "-DVARIANTE=$v"
    open_solution -reset sol
    set_part $part
    create_clock -period $periodo -name default

    if {[catch {csim_design -argv $datos} err]} { puts "AVISO: csim fallo en $proy: $err" }
    csynth_design
    if {$do_cosim} {
        if {[catch {cosim_design -rtl verilog -argv $datos} err]} { puts "AVISO: cosim fallo en $proy: $err" }
    }
    if {[catch {export_design -format ip_catalog -rtl verilog} err]} { puts "AVISO: export fallo en $proy: $err" }

    set out $sm_dir/reportes/v$v
    file mkdir $out
    foreach f [glob -nocomplain $proy/sol/syn/report/csynth.rpt $proy/sol/syn/report/csynth.xml \
                                $proy/sol/syn/report/cgra_e4_csynth.rpt] {
        file copy -force $f $out
    }
    if {[file exists $proy/sol/sim/report]} {
        file delete -force $out/cosim
        file copy -force $proy/sol/sim/report $out/cosim
    }
    close_project
}
exit
