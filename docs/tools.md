---
layout: default
title: Tool Reference
nav_order: 4
permalink: /tools/
---

# Tool Reference
{: .fs-9 .fw-700 }

All available C++ and Java conversion tools.
{: .fs-5 .fw-300 }

---

## C++ Tools (no Java required)

### `bvgraph_gen_offsets`

**Generate `.offsets` file from `.graph` + `.properties` (pure C++).**

```bash
./bvgraph_gen_offsets <basename>
```

- **Input**: `<basename>.graph`, `<basename>.properties`
- **Output**: `<basename>.offsets`
- **Requires**: No Java
- **Speed**: ~5–8 M nodes/s (sequential scan)

This must be run once per graph before using `bvgraph_to_mtx` or `bvgraph_to_bgr`.

---

### `bvgraph_to_mtx`

**Convert BVGraph to MTX format (multi-threaded C++).**

```bash
./bvgraph_to_mtx <basename> <output.mtx> [num_threads]
```

- **Input**: `<basename>.graph`, `.properties`, `.offsets`
- **Output**: Matrix Market text file
- **Default threads**: All available cores
- **Speed**: ~30 M edges/s per thread

Example:
```bash
./bvgraph_to_mtx data/eu-2005 output/eu-2005.mtx 8
```

---

### `bvgraph_to_bgr`

**Convert BVGraph to BGR format (multi-threaded C++).**

```bash
./bvgraph_to_bgr <basename> <output.bgr> [num_threads]
```

- **Input**: `<basename>.graph`, `.properties`, `.offsets`
- **Output**: BGR binary CSR file
- **Default threads**: All available cores
- **Speed**: ~75–120 M edges/s

Example:
```bash
./bvgraph_to_bgr data/indochina-2004 /export/graphs/bgr/indochina-2004.bgr 8
```

---

## Java Tools (require JDK 15+)

### `WG2MTX`

**Convert BVGraph to MTX using the Java WebGraph library.**

```bash
java -Xmx4G -cp jlibs/*:. WG2MTX <basename> <output.mtx>
```

Requires JDK 15+. Uses the official WebGraph `ImmutableGraph` API to iterate all nodes and write edges.

---

### `WG2BGR`

**Convert BVGraph to BGR using the Java WebGraph library.**

```bash
java -Xmx4G -cp jlibs/*:. WG2BGR <basename> <output.bgr>
```

Builds CSR in memory and writes BGR format directly. For very large graphs (>2B edges), increase `-Xmx`.

---

## Validation

### `compare_java_cpp.sh`

**Validate C++ and Java produce identical output on all graphs.**

```bash
./compare_java_cpp.sh                    # test all graphs in data/
./compare_java_cpp.sh cnr-2000 eu-2005   # test specific graphs
```

For each graph, runs both Java and C++ converters for MTX and BGR, then:
- MTX: line-by-line text diff
- BGR: byte-by-byte binary diff

Both must be identical to pass.

---

## Summary

| Tool | Usage | Description |
|------|-------|-------------|
| `bvgraph_gen_offsets` | `./bvgraph_gen_offsets <basename>` | Generate `.offsets` from `.graph` + `.properties` |
| `bvgraph_to_mtx` | `./bvgraph_to_mtx <basename> <out.mtx> [threads]` | Convert to MTX (Matrix Market text) |
| `bvgraph_to_bgr` | `./bvgraph_to_bgr <basename> <out.bgr> [threads]` | Convert to BGR (binary CSR) |
| `WG2MTX` | `java -cp jlibs/*:. WG2MTX <basename> <out.mtx>` | Convert to MTX using WebGraph library |
| `WG2BGR` | `java -cp jlibs/*:. WG2BGR <basename> <out.bgr>` | Convert to BGR using WebGraph library |
| `compare_java_cpp.sh` | `./compare_java_cpp.sh` | Run both Java & C++ on all graphs, diff outputs |
