/*
 * bvgraph_to_mtx.cpp — Convert BVGraph (.graph/.properties/.offsets) to MTX
 *
 * Multi-threaded C++ BVGraph decoder → Matrix Market output.
 *
 * Usage: ./bvgraph_to_mtx <basename> <output.mtx> [num_threads]
 *   basename: path to graph without extension (e.g., data/eu-2005)
 *   output.mtx: output MTX file path
 *   num_threads: optional, defaults to all available cores
 */

#include "bvgraph_reader.h"
#include <omp.h>
#include <chrono>
#include <atomic>
#include <mutex>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <basename> <output.mtx> [num_threads]\n";
        std::cerr << "  basename: path without extension (e.g., data/eu-2005)\n";
        std::cerr << "  output.mtx: output Matrix Market file\n";
        std::cerr << "  num_threads: optional thread count\n";
        return 1;
    }

    std::string basename = argv[1];
    std::string outputFile = argv[2];
    int numThreads = (argc > 3) ? std::atoi(argv[3]) : omp_get_max_threads();
    omp_set_num_threads(numThreads);

    auto t0 = std::chrono::high_resolution_clock::now();

    std::cerr << "=== BVGraph to MTX Converter ===\n";
    std::cerr << "Input:   " << basename << "\n";
    std::cerr << "Output:  " << outputFile << "\n";
    std::cerr << "Threads: " << numThreads << "\n\n";

    // Step 1: Parse properties
    std::cerr << "[1/4] Parsing properties...\n";
    BVGraphProperties props = parseProperties(basename);
    std::cerr << "  Nodes: " << props.nodes << "\n";
    std::cerr << "  Arcs:  " << props.arcs << "\n";
    std::cerr << "  Window size: " << props.windowSize << "\n";
    std::cerr << "  Max ref count: " << props.maxRefCount << "\n";
    std::cerr << "  Min interval length: " << props.minIntervalLength << "\n";
    std::cerr << "  Zeta k: " << props.zetaK << "\n";
    std::cerr << "  Outdegree coding: " << CODING_NAMES[props.outdegreeCoding] << "\n";
    std::cerr << "  Block coding: " << CODING_NAMES[props.blockCoding] << "\n";
    std::cerr << "  Residual coding: " << CODING_NAMES[props.residualCoding] << "\n";
    std::cerr << "  Reference coding: " << CODING_NAMES[props.referenceCoding] << "\n";
    std::cerr << "  Block count coding: " << CODING_NAMES[props.blockCountCoding] << "\n";
    std::cerr << "  Offset coding: " << CODING_NAMES[props.offsetCoding] << "\n\n";

    // Step 2: Load offsets
    std::cerr << "[2/4] Loading offsets...\n";
    auto t1 = std::chrono::high_resolution_clock::now();
    std::vector<int64_t> offsets = loadOffsets(basename, props);
    auto t2 = std::chrono::high_resolution_clock::now();
    std::cerr << "  Loaded " << offsets.size() << " offsets in "
              << std::chrono::duration<double>(t2 - t1).count() << "s\n\n";

    // Step 3: Memory-map graph file
    std::cerr << "[3/4] Memory-mapping graph...\n";
    MappedFile graphFile;
    graphFile.open(basename + ".graph");
    std::cerr << "  Graph file size: " << graphFile.size() << " bytes\n\n";

    // Step 4: Parallel decode and write MTX
    std::cerr << "[4/4] Decoding graph and writing MTX...\n";
    auto t3 = std::chrono::high_resolution_clock::now();

    // Each thread builds a local string buffer of edges
    std::vector<std::string> threadBuffers(numThreads);
    std::vector<int64_t> threadEdgeCounts(numThreads, 0);
    std::atomic<int64_t> progressNodes(0);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int nthreads = omp_get_num_threads();

        int64_t chunkSize = (props.nodes + nthreads - 1) / nthreads;
        int64_t startNode = (int64_t)tid * chunkSize;
        int64_t endNode = std::min(startNode + chunkSize, props.nodes);

        if (startNode >= props.nodes) {
            startNode = endNode = props.nodes;
        }

        // Create a decoder with its own window
        BVGraphDecoder decoder(props, graphFile.data(), graphFile.size(), offsets);

        // Warm up the window for this thread's chunk
        decoder.warmupWindow(startNode);

        // Pre-allocate buffer
        std::string& buf = threadBuffers[tid];
        buf.reserve(1024 * 1024); // 1MB initial

        int64_t edgeCount = 0;
        InputBitStream ibs(graphFile.data(), graphFile.size());

        for (int64_t v = startNode; v < endNode; v++) {
            ibs.position(offsets[v]);
            int64_t degree = decoder.decodeNodeWithWindow(v, ibs);

            // Access the window to get successors
            int cyclicIdx = (int)(v % (props.windowSize + 1));
            const auto& succs = decoder.getWindow(cyclicIdx);

            for (int64_t j = 0; j < degree; j++) {
                // MTX is 1-indexed
                buf += std::to_string(v + 1);
                buf += '\t';
                buf += std::to_string(succs[j] + 1);
                buf += '\n';
            }

            edgeCount += degree;

            int64_t prog = progressNodes.fetch_add(1);
            if (prog % 100000 == 0) {
                #pragma omp critical
                std::cerr << "  Progress: " << prog << " / " << props.nodes
                          << " nodes (" << (100.0 * prog / props.nodes) << "%)\r";
            }
        }

        threadEdgeCounts[tid] = edgeCount;
    }

    std::cerr << "\n";

    // Sum up edge counts
    int64_t totalEdges = 0;
    for (int i = 0; i < numThreads; i++)
        totalEdges += threadEdgeCounts[i];

    std::cerr << "  Total edges decoded: " << totalEdges << "\n";
    if (totalEdges != props.arcs) {
        std::cerr << "  WARNING: Edge count mismatch! Expected " << props.arcs
                  << " but decoded " << totalEdges << "\n";
    }

    // Write MTX file
    std::cerr << "  Writing MTX file...\n";
    FILE* fout = fopen(outputFile.c_str(), "w");
    if (!fout) {
        std::cerr << "Error: Cannot open output file: " << outputFile << "\n";
        return 1;
    }

    // MTX header
    fprintf(fout, "%%%%MatrixMarket matrix coordinate pattern general\n");
    fprintf(fout, "%lld %lld %lld\n", (long long)props.nodes, (long long)props.nodes, (long long)totalEdges);

    // Write all thread buffers
    for (int i = 0; i < numThreads; i++) {
        if (!threadBuffers[i].empty()) {
            fwrite(threadBuffers[i].data(), 1, threadBuffers[i].size(), fout);
        }
    }

    fclose(fout);

    auto t4 = std::chrono::high_resolution_clock::now();
    double totalTime = std::chrono::duration<double>(t4 - t0).count();
    double decodeTime = std::chrono::duration<double>(t4 - t3).count();

    std::cerr << "\n=== Conversion Complete ===\n";
    std::cerr << "Total time: " << totalTime << "s\n";
    std::cerr << "  Decode + write: " << decodeTime << "s\n";
    std::cerr << "  Throughput: " << (totalEdges / decodeTime / 1e6) << " M edges/s\n";

    return 0;
}
