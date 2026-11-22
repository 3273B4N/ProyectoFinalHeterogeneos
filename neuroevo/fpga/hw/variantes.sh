PLATFORM_DIR=/home/esteb/Documents/Semestre_10/Heterogeneos/LabCap4Heterogeneos/FPGA/platform
K=neat_eval_hls
FREQ=200000000

NAMES=(v0_base v1_pipeline v2_lanes2 v2_lanes4 v2_lanes8)
FLAGS=(
  ""
  "USE_PIPELINE=1"
  "USE_PIPELINE=1 USE_LANES=1 NUM_LANES=2"
  "USE_PIPELINE=1 USE_LANES=1 NUM_LANES=4"
  "USE_PIPELINE=1 USE_LANES=1 NUM_LANES=8"
)
