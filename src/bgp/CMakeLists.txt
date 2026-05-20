build_lib(
  LIBNAME bgp
  SOURCE_FILES
    model/bgp.cc
    model/bgp-routing.cc
    model/bgp-ns3-clock.cc
    model/bgp-ns3-fsm.cc
    model/bgp-ns3-socket-in.cc
    model/bgp-ns3-socket-out.cc
    model/bgp-log.cc
  HEADER_FILES
    model/bgp.h
    model/bgp-routing.h
    model/bgp-ns3-clock.h
    model/bgp-ns3-fsm.h
    model/bgp-ns3-socket-in.h
    model/bgp-ns3-socket-out.h
    model/bgp-log.h
  LIBRARIES_TO_LINK ${libcore}
                    ${libinternet}
                    ${libnetwork}
)

# Bundled Nat-Lab libbgp (deps/libbgp submodule). Linked here instead of
# LIBRARIES_TO_LINK to avoid exporting a non-ns-3 target in the ns-3 CMake package.
target_link_libraries(${libbgp} PRIVATE libbgp_bundled)
if(NOT ${XCODE})
  target_link_libraries(${libbgp}-obj PUBLIC libbgp_bundled)
endif()
