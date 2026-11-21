open_project -reset prj_$env(SOL)
set_top neat_eval_hls
add_files neat_eval_hls.cpp -cflags "$env(CFLAGS) -I."
add_files -tb tb.cpp        -cflags "$env(CFLAGS) -I."
open_solution -reset sol1 -flow_target vitis
set_part {xck26-sfvc784-2LV-c}
create_clock -period $env(PERIOD) -name default

csim_design
csynth_design
cosim_design -rtl verilog -tool xsim -trace_level none
exit
