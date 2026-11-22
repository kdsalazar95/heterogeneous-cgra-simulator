# build_bitstream.tcl - Diseño de bloques (PS Zynq MPSoC + IP cgra_e4) y bitstream para PYNQ.
#
# Requisito: run_hls.tcl ya exportó el IP de la variante.
# Uso, desde cgra/fpga:
#     vivado -mode batch -source build_bitstream.tcl -tclargs 2
# Resultado: overlay/cgra_v<k>.bit y overlay/cgra_v<k>.hwh (+ reportes de Vivado).

set var [expr {[llength $argv] > 0 ? [lindex $argv 0] : 2}]
# No se llama "dir": los scripts internos de Vivado sobrescriben esa variable global.
set sm_dir [pwd]
set ipdir  $sm_dir/proy_v$var/sol/impl/ip
set nombre kv260_cgra_v$var
set pdir   $sm_dir/vivado_v$var
if {![file exists $ipdir]} { error "No existe $ipdir: primero corra run_hls.tcl" }

create_project -force $nombre $pdir -part xck26-sfvc784-2LV-c
set bp [get_board_parts -quiet -latest_file_version *kv260_som*]
if {$bp ne ""} { set_property board_part [lindex $bp end] [current_project] }
set_property ip_repo_paths $ipdir [current_project]
update_ip_catalog
create_bd_design design_1

# PS: HPM0_FPD para los registros AXI-Lite, HP0_FPD para que el IP lea/escriba la DDR,
# pl_clk0 a 100 MHz (el mismo periodo de la síntesis HLS).
set ps [create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e zynq_ultra_ps_e_0]
if {$bp ne ""} {
    apply_bd_automation -rule xilinx.com:bd_rule:zynq_ultra_ps_e -config {apply_board_preset "1"} $ps
}
set_property -dict [list CONFIG.PSU__USE__M_AXI_GP0 {1} CONFIG.PSU__USE__M_AXI_GP1 {0} \
    CONFIG.PSU__USE__M_AXI_GP2 {0} CONFIG.PSU__USE__S_AXI_GP2 {1} CONFIG.PSU__FPGA_PL0_ENABLE {1} \
    CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ {100}] $ps

set vlnv [lindex [get_ipdefs -all *cgra_e4*] 0]
create_bd_cell -type ip -vlnv $vlnv cgra_e4_0

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/zynq_ultra_ps_e_0/M_AXI_HPM0_FPD} Slave {/cgra_e4_0/s_axi_control} ddr_seg {Auto} \
    intc_ip {New AXI SmartConnect} master_apm {0}} [get_bd_intf_pins cgra_e4_0/s_axi_control]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/cgra_e4_0/m_axi_gmem0} Slave {/zynq_ultra_ps_e_0/S_AXI_HP0_FPD} ddr_seg {Auto} \
    intc_ip {New AXI SmartConnect} master_apm {0}} [get_bd_intf_pins zynq_ultra_ps_e_0/S_AXI_HP0_FPD]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} \
    Master {/cgra_e4_0/m_axi_gmem1} Slave {/zynq_ultra_ps_e_0/S_AXI_HP0_FPD} ddr_seg {Auto} \
    intc_ip {Auto} master_apm {0}} [get_bd_intf_pins cgra_e4_0/m_axi_gmem1]

assign_bd_address
validate_bd_design
save_bd_design
make_wrapper -files [get_files design_1.bd] -top -import
set_property top design_1_wrapper [current_fileset]

launch_runs impl_1 -to_step write_bitstream -jobs 8
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] ne "100%"} { error "La implementacion fallo" }

file mkdir $sm_dir/overlay
file copy -force $pdir/$nombre.runs/impl_1/design_1_wrapper.bit $sm_dir/overlay/cgra_v$var.bit
set hwh [lindex [glob -nocomplain $pdir/$nombre.gen/sources_1/bd/design_1/hw_handoff/design_1.hwh \
                                  $pdir/$nombre.srcs/sources_1/bd/design_1/hw_handoff/design_1.hwh] 0]
file copy -force $hwh $sm_dir/overlay/cgra_v$var.hwh
open_run impl_1
report_utilization -file $sm_dir/overlay/utilizacion_v$var.rpt
report_timing_summary -file $sm_dir/overlay/timing_v$var.rpt
report_power -file $sm_dir/overlay/potencia_estimada_v$var.rpt
puts "LISTO: overlay/cgra_v$var.bit y overlay/cgra_v$var.hwh"
exit
