#!/bin/bash
#
# compare_java_cpp.sh — Compare Java vs C++ BVGraph decoders edge-by-edge
#
# Runs both Java (WebGraph library) and C++ (bvgraph_reader) converters
# on each graph, then diffs the MTX and BGR outputs.
#
# Usage: ./compare_java_cpp.sh [graph1 graph2 ...]
#   Defaults to all .graph files in data/

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Find JDK 21
if [ -d "/tmp/zulu21.38.21-ca-jdk21.0.5-linux_x64" ]; then
    export JAVA_HOME=/tmp/zulu21.38.21-ca-jdk21.0.5-linux_x64
    export PATH=$JAVA_HOME/bin:$PATH
fi

JAVA_VER=$(java -version 2>&1 | head -1)
echo "Java: $JAVA_VER"
echo "C++:  $(./bvgraph_to_mtx 2>&1 | head -1 || true)"
echo ""

OUTDIR="/tmp/bvgraph_compare"
rm -rf "$OUTDIR"
mkdir -p "$OUTDIR"

# Determine which graphs to test
if [ $# -gt 0 ]; then
    GRAPHS=("$@")
else
    GRAPHS=()
    for f in data/*.graph; do
        base=$(basename "$f" .graph)
        GRAPHS+=("$base")
    done
fi

echo "=========================================="
echo "Graphs to compare: ${GRAPHS[*]}"
echo "=========================================="
echo ""

PASS=0
FAIL=0
TOTAL=0

for g in "${GRAPHS[@]}"; do
    TOTAL=$((TOTAL + 1))
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Graph: $g"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    BASENAME="data/$g"

    # Check files exist
    if [ ! -f "$BASENAME.graph" ] || [ ! -f "$BASENAME.properties" ]; then
        echo "  SKIP: missing .graph or .properties"
        continue
    fi

    # Generate offsets if needed
    if [ ! -f "$BASENAME.offsets" ]; then
        echo "  Generating .offsets..."
        ./bvgraph_gen_offsets "$BASENAME" 2>&1 | tail -1
    fi

    # Print graph stats
    NODES=$(grep "^nodes=" "$BASENAME.properties" | cut -d= -f2)
    ARCS=$(grep "^arcs=" "$BASENAME.properties" | cut -d= -f2)
    printf "  Nodes: %'d, Arcs: %'d\n" "$NODES" "$ARCS"

    # ─── MTX comparison ───
    echo ""
    echo "  [MTX] Java..."
    JAVA_MTX="$OUTDIR/${g}_java.mtx"
    T0=$(date +%s%N)
    java -Xmx4G -cp jlibs/*:. WG2MTX "$BASENAME" "$JAVA_MTX" 2>&1 | grep -E "Time:|edges"
    T1=$(date +%s%N)
    JAVA_MTX_MS=$(( (T1 - T0) / 1000000 ))

    echo "  [MTX] C++..."
    CPP_MTX="$OUTDIR/${g}_cpp.mtx"
    T0=$(date +%s%N)
    ./bvgraph_to_mtx "$BASENAME" "$CPP_MTX" 1 2>&1 | grep -E "Total time|edges"
    T1=$(date +%s%N)
    CPP_MTX_MS=$(( (T1 - T0) / 1000000 ))

    echo "  [MTX] Comparing..."
    JAVA_MTX_LINES=$(wc -l < "$JAVA_MTX")
    CPP_MTX_LINES=$(wc -l < "$CPP_MTX")

    if diff -q "$JAVA_MTX" "$CPP_MTX" > /dev/null 2>&1; then
        echo "  [MTX] ✅ PASS — files identical ($JAVA_MTX_LINES lines)"
        MTX_RESULT="PASS"
    else
        # Line counts may differ due to header format; compare edge content only
        # Extract edges: skip header (lines starting with % or dimension line)
        tail -n +3 "$JAVA_MTX" | sort > "$OUTDIR/${g}_java_edges.txt"
        tail -n +3 "$CPP_MTX" | sort > "$OUTDIR/${g}_cpp_edges.txt"

        if diff -q "$OUTDIR/${g}_java_edges.txt" "$OUTDIR/${g}_cpp_edges.txt" > /dev/null 2>&1; then
            echo "  [MTX] ✅ PASS — edges identical (header differs)"
            MTX_RESULT="PASS"
        else
            DIFF_COUNT=$(diff "$OUTDIR/${g}_java_edges.txt" "$OUTDIR/${g}_cpp_edges.txt" | grep -c "^[<>]" || true)
            echo "  [MTX] ❌ FAIL — $DIFF_COUNT edge differences"
            diff "$OUTDIR/${g}_java_edges.txt" "$OUTDIR/${g}_cpp_edges.txt" | head -20
            MTX_RESULT="FAIL"
        fi
        rm -f "$OUTDIR/${g}_java_edges.txt" "$OUTDIR/${g}_cpp_edges.txt"
    fi

    printf "  [MTX] Java: %'d ms, C++: %'d ms\n" "$JAVA_MTX_MS" "$CPP_MTX_MS"

    # ─── BGR comparison ───
    echo ""
    echo "  [BGR] Java..."
    JAVA_BGR="$OUTDIR/${g}_java.bgr"
    T0=$(date +%s%N)
    java -Xmx4G -cp jlibs/*:. WG2BGR "$BASENAME" "$JAVA_BGR" 2>&1 | grep -E "Done|edges|CSR"
    T1=$(date +%s%N)
    JAVA_BGR_MS=$(( (T1 - T0) / 1000000 ))

    echo "  [BGR] C++..."
    CPP_BGR="$OUTDIR/${g}_cpp.bgr"
    T0=$(date +%s%N)
    ./bvgraph_to_bgr "$BASENAME" "$CPP_BGR" 1 2>&1 | grep -E "Total time|edges"
    T1=$(date +%s%N)
    CPP_BGR_MS=$(( (T1 - T0) / 1000000 ))

    echo "  [BGR] Comparing (binary diff)..."
    if diff -q "$JAVA_BGR" "$CPP_BGR" > /dev/null 2>&1; then
        BGR_SIZE=$(stat -c%s "$CPP_BGR")
        printf "  [BGR] ✅ PASS — files identical (%'d bytes)\n" "$BGR_SIZE"
        BGR_RESULT="PASS"
    else
        JAVA_SIZE=$(stat -c%s "$JAVA_BGR")
        CPP_SIZE=$(stat -c%s "$CPP_BGR")
        printf "  [BGR] ❌ FAIL — Java: %'d bytes, C++: %'d bytes\n" "$JAVA_SIZE" "$CPP_SIZE"
        # Show where they differ
        cmp -l "$JAVA_BGR" "$CPP_BGR" 2>/dev/null | head -10
        BGR_RESULT="FAIL"
    fi

    printf "  [BGR] Java: %'d ms, C++: %'d ms\n" "$JAVA_BGR_MS" "$CPP_BGR_MS"

    # ─── Summary for this graph ───
    echo ""
    if [ "$MTX_RESULT" = "PASS" ] && [ "$BGR_RESULT" = "PASS" ]; then
        echo "  ✅ $g: ALL PASS"
        PASS=$((PASS + 1))
    else
        echo "  ❌ $g: MTX=$MTX_RESULT, BGR=$BGR_RESULT"
        FAIL=$((FAIL + 1))
    fi
    echo ""

    # Cleanup large files
    rm -f "$JAVA_MTX" "$CPP_MTX" "$JAVA_BGR" "$CPP_BGR"
done

echo "=========================================="
echo "RESULTS: $PASS/$TOTAL passed, $FAIL failed"
echo "=========================================="

rmdir "$OUTDIR" 2>/dev/null || true

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
