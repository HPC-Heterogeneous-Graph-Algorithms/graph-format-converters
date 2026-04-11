# Graph Format Converters

Convert [WebGraph BVGraph](https://webgraph.di.unimi.it/) compressed graphs (`.graph` + `.properties`) from the [LAW dataset collection](http://law.di.unimi.it/datasets.php) into standard formats: **MTX** (Matrix Market) and **BGR** (Binary Graph Representation / CSR).

Includes both a **pure C++ multi-threaded decoder** (no Java needed) and **Java converters** using the official WebGraph library, with a comparison script to verify byte-identical output.

## Quick Start

```bash
# 1. Build C++ tools
make cpp

# 2. Download a graph (example: eu-2005, 862K nodes, 19M edges)
mkdir -p data
wget -P data http://data.law.di.unimi.it/webdata/eu-2005/eu-2005.graph
wget -P data http://data.law.di.unimi.it/webdata/eu-2005/eu-2005.properties

# 3. Generate .offsets file (pure C++, no Java)
./bvgraph_gen_offsets data/eu-2005

# 4. Convert to BGR (binary CSR)
./bvgraph_to_bgr data/eu-2005 data/eu-2005.bgr 8

# 5. Or convert to MTX (text edge list)
./bvgraph_to_mtx data/eu-2005 data/eu-2005.mtx 8
```

## Dependencies

### C++ tools (recommended — no Java required)

| Dependency | Version | Purpose |
|------------|---------|---------|
| `g++` | 9+ | C++17 compiler |
| OpenMP | (bundled with g++) | Multi-threaded parallelism |

Install on Ubuntu/Debian:
```bash
sudo apt install g++
```

### Java tools (optional — for comparison/validation)

| Dependency | Version | Purpose |
|------------|---------|---------|
| JDK | 15+ (21 recommended) | Compile & run Java converters |
| WebGraph jars | (auto-downloaded) | Official BVGraph library |

The WebGraph Java libraries are auto-downloaded on first `make` if not present.

## Input Files

Each LAW dataset consists of three files (all sharing the same basename):

| File | Description |
|------|-------------|
| `<name>.graph` | Compressed bitstream (BVGraph format: gamma/delta/zeta coded successor lists) |
| `<name>.properties` | Parameters: node count, arc count, window size, compression flags |
| `<name>.offsets` | Bit positions for each node in the `.graph` file (enables random access / parallelism) |

The `.offsets` file can be generated from the other two using `bvgraph_gen_offsets`.

### Example Dataset Downloads

```bash
mkdir -p data

# Small (325K nodes, 3.2M edges, ~1 MB)
wget -P data http://data.law.di.unimi.it/webdata/cnr-2000/cnr-2000.graph
wget -P data http://data.law.di.unimi.it/webdata/cnr-2000/cnr-2000.properties

# Medium (862K nodes, 19M edges, ~9 MB)
wget -P data http://data.law.di.unimi.it/webdata/eu-2005/eu-2005.graph
wget -P data http://data.law.di.unimi.it/webdata/eu-2005/eu-2005.properties

# Large (7.4M nodes, 194M edges, ~34 MB)
wget -P data http://data.law.di.unimi.it/webdata/indochina-2004/indochina-2004.graph
wget -P data http://data.law.di.unimi.it/webdata/indochina-2004/indochina-2004.properties

# Browse all datasets: http://law.di.unimi.it/datasets.php
```

## Tools Reference

### C++ Tools (no Java required)

| Tool | Usage | Description |
|------|-------|-------------|
| `bvgraph_gen_offsets` | `./bvgraph_gen_offsets <basename>` | Generate `.offsets` from `.graph` + `.properties` |
| `bvgraph_to_mtx` | `./bvgraph_to_mtx <basename> <out.mtx> [threads]` | Convert to MTX (Matrix Market text) |
| `bvgraph_to_bgr` | `./bvgraph_to_bgr <basename> <out.bgr> [threads]` | Convert to BGR (binary CSR) |

### Java Tools (require JDK 15+)

| Tool | Usage | Description |
|------|-------|-------------|
| `WG2MTX` | `java -cp jlibs/*:. WG2MTX <basename> <out.mtx>` | Convert to MTX using WebGraph library |
| `WG2BGR` | `java -cp jlibs/*:. WG2BGR <basename> <out.bgr>` | Convert to BGR using WebGraph library |
| `WG2Bin` | `make WG2Bin args="<basename> <outdir>"` | Convert to intermediate binary (legacy) |

### Validation

| Tool | Usage | Description |
|------|-------|-------------|
| `compare_java_cpp.sh` | `./compare_java_cpp.sh` | Run both Java & C++ on all graphs, diff outputs |

## Output Formats

| Format | Type | Node IDs | Details |
|--------|------|----------|---------|
| **MTX** | Text (Matrix Market) | 1-indexed | [MTX format spec](https://hpc-heterogeneous-graph-algorithms.github.io/Graph-Formats/mtx) |
| **BGR** | Binary (CSR) | 0-indexed | [BGR format spec](https://hpc-heterogeneous-graph-algorithms.github.io/Graph-Formats/bgr) |

Input BVGraph format documentation: [BVGraph format spec](https://hpc-heterogeneous-graph-algorithms.github.io/Graph-Formats/bvgraph)

## Build

```bash
# Build everything (C++ + Java)
make all

# Build C++ tools only
make cpp

# Build and run comparison test
make test-cpp
```

## Performance

Benchmarked on eu-2005 (862K nodes, 19M edges), single thread:

| Pipeline | MTX time | BGR time |
|----------|----------|----------|
| C++ | 0.68s | 0.26s |
| Java | 1.76s | 0.52s |
| **Speedup** | **2.6×** | **2.0×** |

Multi-threaded C++ (4 threads): BGR in 0.13s (~150M edges/s).

## Full Documentation

See the **[full documentation](https://hpc-heterogeneous-graph-algorithms.github.io/graph-format-converters/)** for:
- Architecture & how the BVGraph decoder works
- Detailed file format specifications
- Per-tool deep-dive with examples
- Troubleshooting & FAQ