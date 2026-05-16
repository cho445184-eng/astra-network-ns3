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
                    bgp
)
