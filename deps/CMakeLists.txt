# Bundled Nat-Lab libbgp (submodule at deps/libbgp, tag 0.6.3).
# Provides target libbgp_bundled (alias libbgp::libbgp) for src/bgp and point-to-point.

set(_libbgp_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/libbgp/src")

set(_libbgp_source_names
    bgp-bad-message.cc
    bgp-capability.cc
    bgp-errcode.cc
    bgp-filter.cc
    bgp-fsm.cc
    bgp-keepalive-message.cc
    bgp-log-handler.cc
    bgp-notification-message.cc
    bgp-open-message.cc
    bgp-packet.cc
    bgp-path-attrib.cc
    bgp-rib4.cc
    bgp-rib6.cc
    bgp-sink.cc
    bgp-update-message.cc
    fd-out-handler.cc
    prefix4.cc
    prefix6.cc
    realtime-clock.cc
    route-event-bus.cc
    serializable.cc
)

set(_libbgp_headers
    bgp-afi.h
    bgp-bad-message.h
    bgp-capability.h
    bgp-config.h
    bgp-errcode.h
    bgp-filter.h
    bgp-fsm.h
    bgp-keepalive-message.h
    bgp-log-handler.h
    bgp-message.h
    bgp-notification-message.h
    bgp-open-message.h
    bgp-out-handler.h
    bgp-packet.h
    bgp-path-attrib.h
    bgp-rib.h
    bgp-rib4.h
    bgp-rib6.h
    bgp-sink.h
    bgp-update-message.h
    bgp.h
    clock.h
    fd-out-handler.h
    prefix.h
    prefix4.h
    prefix6.h
    realtime-clock.h
    route-event-bus.h
    route-event-receiver.h
    route-event.h
    serializable.h
    value-op.h
)

set(_libbgp_include_root "${CMAKE_CURRENT_BINARY_DIR}/include")
file(MAKE_DIRECTORY "${_libbgp_include_root}/libbgp")
foreach(_hdr IN LISTS _libbgp_headers)
    configure_file(
        "${_libbgp_src_dir}/${_hdr}"
        "${_libbgp_include_root}/libbgp/${_hdr}"
        COPYONLY
    )
endforeach()

set(_libbgp_sources "")
foreach(_src IN LISTS _libbgp_source_names)
    list(APPEND _libbgp_sources "${_libbgp_src_dir}/${_src}")
endforeach()

add_library(libbgp_bundled STATIC ${_libbgp_sources})
target_include_directories(
    libbgp_bundled
    PUBLIC
    "${_libbgp_include_root}"
)
set_target_properties(
    libbgp_bundled
    PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

add_library(libbgp::libbgp ALIAS libbgp_bundled)
