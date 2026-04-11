---
layout: default
title: Formats
nav_order: 3
permalink: /formats/
---

# Input & Output Formats
{: .fs-9 .fw-700 }

BVGraph compressed input and MTX / BGR output formats.
{: .fs-5 .fw-300 }

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

📖 **Full format specification**: [BVGraph format details](https://hpc-heterogeneous-graph-algorithms.github.io/Graph-Formats/bvgraph)

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

📖 **Full format specification**: [MTX format details](https://hpc-heterogeneous-graph-algorithms.github.io/Graph-Formats/mtx)

### BGR (Binary Graph Representation)

A compact binary CSR (Compressed Sparse Row) format with adaptive integer sizes (uint32/uint64), a 1-byte header with bit flags, and parallel-friendly I/O layout. Unweighted graphs store only `row_ptr` and `col_idx` arrays with **0-indexed** node IDs.

📖 **Full format specification**: [BGR format details](https://hpc-heterogeneous-graph-algorithms.github.io/Graph-Formats/bgr)
