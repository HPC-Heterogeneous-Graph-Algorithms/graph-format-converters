---
layout: default
title: Benchmarks
nav_order: 6
permalink: /benchmarks/
---

# Validation & Benchmarks
{: .fs-9 .fw-700 }

Edge-by-edge correctness verification and performance comparison.
{: .fs-5 .fw-300 }

---

## Edge-by-Edge Comparison (5 graphs, all PASS)

| Graph | Nodes | Arcs | MTX | BGR |
|-------|------:|-----:|:---:|:---:|
| cnr-2000 | 325,557 | 3,216,152 | ✅ identical | ✅ identical |
| eu-2005 | 862,664 | 19,235,140 | ✅ identical | ✅ identical |
| in-2004 | 1,382,908 | 16,917,053 | ✅ identical | ✅ identical |
| indochina-2004 | 7,414,866 | 194,109,311 | ✅ identical | ✅ identical |
| uk-2007-05@100K | 100,000 | 3,050,615 | ✅ identical | ✅ identical |

---

## Performance Comparison (single thread)

| Graph | C++ MTX | Java MTX | C++ BGR | Java BGR |
|-------|--------:|---------:|--------:|---------:|
| cnr-2000 | 0.13s | 0.47s | 0.05s | 0.24s |
| eu-2005 | 0.68s | 1.76s | 0.26s | 0.52s |
| in-2004 | 0.61s | 1.59s | 0.22s | 0.48s |
| indochina-2004 | 6.63s | 14.27s | 1.67s | 2.58s |
| uk-2007-05@100K | 0.10s | 0.42s | 0.03s | 0.20s |

C++ is consistently **2–6× faster** than Java.

---

## Multi-threaded Performance

Multi-threaded C++ (4 threads) on eu-2005: BGR in **0.13s** (~150M edges/s).

---

## Validation Method

The `compare_java_cpp.sh` script validates correctness by running both Java and C++ converters on the same graphs:

- **MTX**: line-by-line text diff
- **BGR**: byte-by-byte binary diff

Both outputs must be **byte-identical** to pass. See [Tool Reference → compare_java_cpp.sh]({{ site.baseurl }}/tools/#compare_java_cppsh) for usage.
