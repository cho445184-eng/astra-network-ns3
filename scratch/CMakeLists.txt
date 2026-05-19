set(astra-sim-ns3-libs-list
  "libapplications"
  "libbgp"
  "libcore"
  "libcsma"
  "libinternet"
  "libpoint-to-point"
  "libnetwork"
)
list(JOIN astra-sim-ns3-libs-list " " astra-sim-ns3-libs)
set(astra-sim-dir "${PROJECT_SOURCE_DIR}/../../../")
include_directories("../scratch")

# AstraSim from repo root via shared cmake module (no add_subdirectory of parent project).
set(ASTRA_SIM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../")
include("${ASTRA_SIM_ROOT}/cmake/AstraSimLibrary.cmake")

set(astra-sim-exec-libs ${astra-sim-ns3-libs-list} AstraSim)
if(TARGET AstraSimProtobufDeps)
  list(APPEND astra-sim-exec-libs AstraSimProtobufDeps)
endif()

build_exec(
  EXECNAME AstraSimNetwork
  SOURCE_FILES ${astra-sim-dir}/astra-sim/network_frontend/ns3/AstraSimNetwork.cc
  DIRECTORY_PATHS ${astra-sim-dir}/extern/helper
  LIBRARIES_TO_LINK "${astra-sim-exec-libs}"
  EXECUTABLE_DIRECTORY_PATH "${PROJECT_SOURCE_DIR}/build/scratch/"
)
