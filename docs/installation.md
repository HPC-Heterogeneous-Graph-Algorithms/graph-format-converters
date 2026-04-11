---
layout: default
title: Installation
nav_order: 2
permalink: /installation/
---

# Installation
{: .fs-9 .fw-700 }

Build the C++ tools (recommended) or optionally the Java converters.
{: .fs-5 .fw-300 }

---

## C++ Tools (recommended)

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

---

## Java Tools (optional)

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

## Build Commands

```bash
# Build everything (C++ + Java)
make all

# Build C++ tools only
make cpp

# Build and run comparison test
make test-cpp
```

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
