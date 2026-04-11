---
layout: default
title: Home
nav_order: 1
permalink: /
---

# Graph Format Converters
{: .fs-9 .fw-700 }

Convert [WebGraph BVGraph](https://webgraph.di.unimi.it/) compressed graphs from the [LAW dataset collection](http://law.di.unimi.it/datasets.php) into **MTX** and **BGR** formats using multi-threaded C++ or Java.
{: .fs-5 .fw-300 }

[Get Started →]({{ site.baseurl }}/installation/){: .btn .btn-primary .fs-5 .mb-4 .mb-md-0 .mr-2 }
[Graph Formats →](https://hpc-heterogeneous-graph-algorithms.github.io/Graph-Formats/){: .btn .btn-outline .fs-5 .mb-4 .mb-md-0 }

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

See [Installation]({{ site.baseurl }}/installation/) for full setup details and [Tool Reference]({{ site.baseurl }}/tools/) for all available tools.

---

## Performance Highlights

C++ is consistently **2–6× faster** than Java:

| Graph | C++ MTX | Java MTX | C++ BGR | Java BGR |
|-------|--------:|---------:|--------:|---------:|
| eu-2005 | 0.68s | 1.76s | 0.26s | 0.52s |
| indochina-2004 | 6.63s | 14.27s | 1.67s | 2.58s |

Multi-threaded C++ (4 threads): BGR in 0.13s (~150M edges/s).

See [Benchmarks]({{ site.baseurl }}/benchmarks/) for full results across 5 datasets.

---

## Repository

```
bvgraph_reader.h         ← Header-only C++ BVGraph decoder
bvgraph_gen_offsets.cpp   ← Generate .offsets (pure C++)
bvgraph_to_mtx.cpp        ← BVGraph → MTX converter
bvgraph_to_bgr.cpp        ← BVGraph → BGR converter
WG2MTX.java / WG2BGR.java ← Java converters (optional)
compare_java_cpp.sh        ← Validation script
Makefile                   ← Build system
```
