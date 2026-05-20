ns3-bgp
---

`ns3-bgp` is a BGP speaker application for `ns-3`.  It is built on top of [libbgp](https://github.com/Nat-Lab/libbgp). For simple usage and quick start, refer to examples. For detailed API usages, refer to document.

### Install

In this fork, [libbgp](https://github.com/Nat-Lab/libbgp/) 0.6.3 is bundled as a git submodule at `deps/libbgp` and built automatically via `deps/CMakeLists.txt` (no system install required). Initialize it with `git submodule update --init deps/libbgp` from the ns-3 tree.

Upstream `ns3-bgp` install instructions (system libbgp) for reference:

```
$ git clone https://github.com/Nat-Lab/ns3-bgp src/bgp
$ ./waf configure
```

### Log Components

The following log components are avaliable.

- `Bgp`: The BGP speaker application.
- `BgpLog`: The log forwarder to forward `libbgp` log to `ns-3`
- `BgpRouting`: The BGP routing protocol (`ns3::Ipv4RoutingProtocol`).


### API Document

`ns3-bgp` document is avaliable online at <https://lab.nat.moe/ns3bgp-doc/>. Examples can be found under `examples/` directory.

### License

MIT
