---
layout: default
title: Decoder Internals
nav_order: 5
permalink: /decoder/
---

# How the C++ Decoder Works
{: .fs-9 .fw-700 }

Architecture of the header-only BVGraph decoder in `bvgraph_reader.h`.
{: .fs-5 .fw-300 }

---

## Overview

The decoder is implemented in `bvgraph_reader.h` (header-only) and consists of four main components:

1. **InputBitStream** — bit-level I/O
2. **Coding Functions** — integer decompression
3. **BVGraphDecoder** — successor list reconstruction
4. **Parallel Decompression** — multi-threaded conversion

---

## 1. InputBitStream

Reads individual bits from a memory-mapped byte array (MSB-first per byte). Provides:
- `readBit()`, `readBits(n)` — raw bit I/O
- `position(bitPos)` / `position()` — seek/tell

---

## 2. Coding Functions

Implements all WebGraph integer coding schemes:

| Function | Encoding |
|----------|----------|
| `readUnary()` | Count zeros until a 1-bit |
| `readGamma()` | Unary-coded length + binary value |
| `readDelta()` | Gamma-coded length + binary value |
| `readZeta(k)` | Parameterized code with shrinking factor k |
| `readNibble()` | 4-bit chunks with continuation bits |

Each has 32-bit and 64-bit (`readLong*`) variants.

---

## 3. BVGraphDecoder

Decodes one node's successor list at a time. Maintains a **cyclic window** of size `windowSize + 1` storing recently decoded successor lists for reference resolution.

Key method: `decodeNodeWithWindow(nodeId, bitstream)`:
1. Read outdegree
2. Read reference → look up referenced node's successors in window
3. Read copy blocks → select which referenced successors to copy
4. Read intervals → decode contiguous ranges
5. Read residuals → decode gap-coded scattered successors
6. Three-way merge (block-copied + intervals + residuals) → sorted successor list

---

## 4. Parallel Decompression

The `.offsets` file provides the bit position of each node in the `.graph` file, enabling parallelism:

1. Divide nodes into chunks (one per thread)
2. Each thread **warms up** its window by decoding `windowSize` nodes before its chunk start
3. Each thread independently decodes its chunk using its own window
4. Results are written into pre-allocated CSR arrays using computed offsets
