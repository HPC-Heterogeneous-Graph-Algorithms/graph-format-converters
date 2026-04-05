---
layout: default
title: Graph Format Converters — Documentation
---

# Graph Format Converters — Documentation

Convert [WebGraph BVGraph](https://webgraph.di.unimi.it/) compressed graphs from the [LAW dataset collection](http://law.di.unimi.it/datasets.php) into **MTX** and **BGR** formats using multi-threaded C++ or Java.

---

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Installation](#installation)
- [Input: BVGraph Format](#input-bvgraph-format)
- [Output Formats](#output-formats)
- [Tool Reference](#tool-reference)
  - [bvgraph_gen_offsets (C++)](#bvgraph_gen_offsets)
  - [bvgraph_to_mtx (C++)](#bvgraph_to_mtx)
  - [bvgraph_to_bgr (C++)](#bvgraph_to_bgr)
  - [WG2MTX (Java)](#wg2mtx)
  - [WG2BGR (Java)](#wg2bgr)
  - [compare_java_cpp.sh](#compare_java_cppsh)
- [How the C++ Decoder Works](#how-the-c-decoder-works)
- [Downloading Graphs](#downloading-graphs)
- [Validation & Benchmarks](#validation--benchmarks)
- [Troubleshooting](#troubleshooting)

---

## Architecture Overview

```
                    LAW Dataset
                   ┌───────────┐
                   │ .graph    │  Compressed bitstream (BVGraph format)
                   │ .properties│  Graph metadata and compression parameters
                   └─────┬─────┘
                         │
              ┌──────────┴──────────┐
              │                     │
        C++ Pipeline          Java Pipeline
        (no Java needed)      (JDK 15+ required)
              │                     │
     ┌────────┴────────┐        ┌───┴───────┐
     │                 │        │           │
 bvgraph_gen_offsets   │        │           │
     │ (generates      │        │           │
     │  .offsets)      │        │           │
     │                 │        │           │
bvgraph_to_mt bvgraph_to_bgr WG2MTX       WG2BGR
     │                 │        │           │
     ▼                 ▼        ▼           ▼
  .mtx file        .bgr file  .mtx file  .bgr file
 (text edges)    (binary CSR) (text)     (binary CSR)
```

Both pipelines produce **byte-identical** output, verified across 5 datasets.

---

## Installation

### C++ Tools (recommended)

Requirements:
- **g++ 9+** with C++17 support
- **OpenMP** (included with g++ on most systems)

```bash
# Ubuntu/Debian
sudo apt install g++

# Build all C++ tools
make cpp
```

This produces three executables: `bvgraph_gen_offsets`, `bvgraph_to_mtx`, `bvgraph_to_bgr`.

### Java Tools (optional)

Requirements:
- **JDK 15+** (JDK 21 recommended)

```bash
# Ubuntu/Debian
sudo apt install openjdk-21-jdk

# Or download from https://adoptium.net/ or https://www.azul.com/downloads/

# Build everything (C++ + Java)
make all
```

The WebGraph Java libraries (`jlibs/` folder) are automatically downloaded on first build if not present.

---

## Input: BVGraph Format

The [WebGraph BVGraph format](https://webgraph.di.unimi.it/) is a highly compressed graph representation developed by Paolo Boldi and Sebastiano Vigna. It uses gamma, delta, and zeta codes with reference-based copy compression to achieve very compact storage of web graphs and social networks.

Each dataset consists of three files sharing a common basename:

| File | Description |
|------|-------------|
| `<name>.graph` | Compressed bitstream containing all successor lists |
| `<name>.properties` | Graph metadata: node/arc counts, compression parameters |
| `<name>.offsets` | Bit positions for each node (enables random access & parallelism) |

The `.offsets` file can be generated from the other two:
```bash
./bvgraph_gen_offsets data/eu-2005      # C++ (no Java)
# or: java -cp jlibs/*: it.unimi.dsi.webgraph.BVGraph data/eu-2005 -O
```

📖 **Full format specification**: [BVGraph format details](https://hpc-heterogeneous-graph-algorithms.github.io/graph-formats/bvgraph)

---

## Output Formats

This tool produces two output formats:

### MTX (Matrix Market)

A standard text-based sparse matrix format. Each line is a `source  destination` edge pair with **1-indexed** node IDs.

```
%%MatrixMarket matrix coordinate pattern general
862664 862664 19235140
1	1
1	2
2	2
...
```

📖 **Full format specification**: [MTX format details](https://hpc-heterogeneous-graph-algorithms.github.io/graph-formats/mtx)

### BGR (Binary Graph Representation)

A compact binary CSR (Compressed Sparse Row) format with adaptive integer sizes (uint32/uint64), a 1-byte header with bit flags, and parallel-friendly I/O layout. Unweighted graphs store only `row_ptr` and `col_idx` arrays with **0-indexed** node IDs.

📖 **Full format specification**: [BGR format details](https://hpc-heterogeneous-graph-algorithms.github.io/graph-formats/bgr)

---

## Tool Reference

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

## How the C++ Decoder Works

The decoder is implemented in `bvgraph_reader.h` (header-only) and consists of:

### 1. InputBitStream

Reads individual bits from a memory-mapped byte array (MSB-first per byte). Provides:
- `readBit()`, `readBits(n)` — raw bit I/O
- `position(bitPos)` / `position()` — seek/tell

### 2. Coding Functions

Implements all WebGraph integer coding schemes:

| Function | Encoding |
|----------|----------|
| `readUnary()` | Count zeros until a 1-bit |
| `readGamma()` | Unary-coded length + binary value |
| `readDelta()` | Gamma-coded length + binary value |
| `readZeta(k)` | Parameterized code with shrinking factor k |
| `readNibble()` | 4-bit chunks with continuation bits |

Each has 32-bit and 64-bit (`readLong*`) variants.

### 3. BVGraphDecoder

Decodes one node's successor list at a time. Maintains a **cyclic window** of size `windowSize + 1` storing recently decoded successor lists for reference resolution.

Key method: `decodeNodeWithWindow(nodeId, bitstream)`:
1. Read outdegree
2. Read reference → look up referenced node's successors in window
3. Read copy blocks → select which referenced successors to copy
4. Read intervals → decode contiguous ranges
5. Read residuals → decode gap-coded scattered successors
6. Three-way merge (block-copied + intervals + residuals) → sorted successor list

### 4. Parallel Decompression

The `.offsets` file provides the bit position of each node in the `.graph` file, enabling parallelism:

1. Divide nodes into chunks (one per thread)
2. Each thread **warms up** its window by decoding `windowSize` nodes before its chunk start
3. Each thread independently decodes its chunk using its own window
4. Results are written into pre-allocated CSR arrays using computed offsets

---

## Downloading Graphs

All LAW datasets are available at [http://law.di.unimi.it/datasets.php](http://law.di.unimi.it/datasets.php).

### Download Pattern

```bash
BASE_URL="http://data.law.di.unimi.it/webdata"
GRAPH="eu-2005"

wget -P data "$BASE_URL/$GRAPH/$GRAPH.graph"
wget -P data "$BASE_URL/$GRAPH/$GRAPH.properties"
```

### Full Pipeline Example

```bash
# 1. Build
make cpp

# 2. Download
mkdir -p data
wget -P data http://data.law.di.unimi.it/webdata/indochina-2004/indochina-2004.graph
wget -P data http://data.law.di.unimi.it/webdata/indochina-2004/indochina-2004.properties

# 3. Generate offsets
./bvgraph_gen_offsets data/indochina-2004

# 4. Convert to BGR
./bvgraph_to_bgr data/indochina-2004 /export/graphs/bgr/indochina-2004.bgr 8
```

---

## Validation & Benchmarks

### Edge-by-Edge Comparison (5 graphs, all PASS)

| Graph | Nodes | Arcs | MTX | BGR |
|-------|------:|-----:|:---:|:---:|
| cnr-2000 | 325,557 | 3,216,152 | ✅ identical | ✅ identical |
| eu-2005 | 862,664 | 19,235,140 | ✅ identical | ✅ identical |
| in-2004 | 1,382,908 | 16,917,053 | ✅ identical | ✅ identical |
| indochina-2004 | 7,414,866 | 194,109,311 | ✅ identical | ✅ identical |
| uk-2007-05@100K | 100,000 | 3,050,615 | ✅ identical | ✅ identical |

### Performance Comparison (single thread)

| Graph | C++ MTX | Java MTX | C++ BGR | Java BGR |
|-------|--------:|---------:|--------:|---------:|
| cnr-2000 | 0.13s | 0.47s | 0.05s | 0.24s |
| eu-2005 | 0.68s | 1.76s | 0.26s | 0.52s |
| in-2004 | 0.61s | 1.59s | 0.22s | 0.48s |
| indochina-2004 | 6.63s | 14.27s | 1.67s | 2.58s |
| uk-2007-05@100K | 0.10s | 0.42s | 0.03s | 0.20s |

C++ is consistently **2–6× faster** than Java.

---

## Troubleshooting

### "Cannot open file: data/xxx.offsets"

The `.offsets` file must be generated before conversion. Run:
```bash
./bvgraph_gen_offsets data/xxx
```

### Edge count mismatch warning

If the C++ decoder reports a different edge count than the `.properties` file, the `.offsets` file may be corrupt. Regenerate it:
```bash
rm data/xxx.offsets
./bvgraph_gen_offsets data/xxx
```

### Java "UnsupportedClassVersionError"

The Java converters require JDK 15+. Check your version:
```bash
java -version
```

### Out of memory for large graphs

For graphs with billions of edges, the in-memory CSR arrays can be very large. Ensure sufficient RAM:
- ~16 bytes per edge (8 for col_idx + 8 for row_ptr amortized)
- indochina-2004 (194M edges): ~3 GB RAM
- uk-2014 (47B edges): ~750 GB RAM

For Java, increase heap size:
```bash
java -Xmx64G -cp jlibs/*:. WG2BGR data/large-graph output.bgr
```

### Building with older g++

Requires g++ 9+ for C++17 `<filesystem>` and structured bindings. If using g++ 7/8:
```bash
g++ -std=c++17 -lstdc++fs ...
```

---

## References

- P. Boldi, S. Vigna. [The WebGraph Framework I: Compression Techniques](http://vigna.di.unimi.it/papers.php#BoVWFI). WWW 2004.
- [WebGraph Java library](https://github.com/vigna/webgraph)
- [WebGraph Rust library](https://crates.io/crates/webgraph)
- [LAW datasets](http://law.di.unimi.it/datasets.php)
- [DSI Utilities (InputBitStream)](https://github.com/vigna/dsiutils)
