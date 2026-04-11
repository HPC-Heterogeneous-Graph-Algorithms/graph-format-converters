---
layout: default
title: Troubleshooting
nav_order: 7
permalink: /troubleshooting/
---

# Troubleshooting
{: .fs-9 .fw-700 }

Common issues and how to resolve them.
{: .fs-5 .fw-300 }

---

### "Cannot open file: data/xxx.offsets"

The `.offsets` file must be generated before conversion. Run:
```bash
./bvgraph_gen_offsets data/xxx
```

---

### Edge count mismatch warning

If the C++ decoder reports a different edge count than the `.properties` file, the `.offsets` file may be corrupt. Regenerate it:
```bash
rm data/xxx.offsets
./bvgraph_gen_offsets data/xxx
```

---

### Java "UnsupportedClassVersionError"

The Java converters require JDK 15+. Check your version:
```bash
java -version
```

---

### Out of memory for large graphs

For graphs with billions of edges, the in-memory CSR arrays can be very large. Ensure sufficient RAM:
- ~16 bytes per edge (8 for col_idx + 8 for row_ptr amortized)
- indochina-2004 (194M edges): ~3 GB RAM
- uk-2014 (47B edges): ~750 GB RAM

For Java, increase heap size:
```bash
java -Xmx64G -cp jlibs/*:. WG2BGR data/large-graph output.bgr
```

---

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
