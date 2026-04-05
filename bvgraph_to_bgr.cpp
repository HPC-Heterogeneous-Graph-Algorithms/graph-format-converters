/*
 * bvgraph_to_bgr.cpp — Convert BVGraph (.graph/.properties/.offsets) to BGR
 *
 * Multi-threaded C++ BVGraph decoder → BGR (Binary Graph Representation) format.
 * Skips intermediate MTX text format for much better performance on large graphs.
 *
 * Usage: ./bvgraph_to_bgr <basename> <output.bgr> [num_threads]
 */

#include "bvgraph_reader.h"
#include <omp.h>
#include <chrono>
#include <atomic>
#include <fcntl.h>
#include <unistd.h>

// BGR format header flags and helpers (from /export/graphs/bgr/docs/bgr_format.md)
struct BGRHeader {
    uint8_t  flags    = 0;
    bool     nodeU64  = false;
    bool     edgeU64  = false;
    bool     weighted = false;
    uint64_t numNodes = 0;
    uint64_t numEdges = 0;

    size_t nodeBytes() const { return nodeU64 ? 8 : 4; }
    size_t edgeBytes() const { return edgeU64 ? 8 : 4; }
    size_t rowPtrOffset() const { return 1 + nodeBytes() + edgeBytes(); }
    size_t colIdxOffset() const { return rowPtrOffset() + edgeBytes() * numNodes; }
    size_t totalFileSize() const {
        return 1 + nodeBytes() + edgeBytes()
             + edgeBytes() * numNodes   // row_ptr
             + nodeBytes() * numEdges;  // col_idx
    }
};

BGRHeader makeBGRHeader(uint64_t numNodes, uint64_t numEdges) {
    BGRHeader h;
    h.numNodes = numNodes;
    h.numEdges = numEdges;
    h.nodeU64 = (numNodes > 0xFFFFFFFFULL);
    h.edgeU64 = (numEdges > 0xFFFFFFFFULL);
    h.weighted = false;
    h.flags = (h.nodeU64 ? 1 : 0) | (h.edgeU64 ? 2 : 0);
    return h;
}

size_t encodeBGRMeta(const BGRHeader& h, uint8_t* buf) {
    size_t off = 0;
    buf[off++] = h.flags;
    if (h.nodeU64) { memcpy(buf + off, &h.numNodes, 8); off += 8; }
    else { uint32_t v = (uint32_t)h.numNodes; memcpy(buf + off, &v, 4); off += 4; }
    if (h.edgeU64) { memcpy(buf + off, &h.numEdges, 8); off += 8; }
    else { uint32_t v = (uint32_t)h.numEdges; memcpy(buf + off, &v, 4); off += 4; }
    return off;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <basename> <output.bgr> [num_threads]\n";
        return 1;
    }

    std::string basename = argv[1];
    std::string outputFile = argv[2];
    int numThreads = (argc > 3) ? std::atoi(argv[3]) : omp_get_max_threads();
    omp_set_num_threads(numThreads);

    auto t0 = std::chrono::high_resolution_clock::now();

    std::cerr << "=== BVGraph to BGR Converter ===\n";
    std::cerr << "Input:   " << basename << "\n";
    std::cerr << "Output:  " << outputFile << "\n";
    std::cerr << "Threads: " << numThreads << "\n\n";

    // Step 1: Parse properties
    std::cerr << "[1/5] Parsing properties...\n";
    BVGraphProperties props = parseProperties(basename);
    std::cerr << "  Nodes: " << props.nodes << ", Arcs: " << props.arcs << "\n\n";

    // Step 2: Load offsets
    std::cerr << "[2/5] Loading offsets...\n";
    auto t1 = std::chrono::high_resolution_clock::now();
    std::vector<int64_t> offsets = loadOffsets(basename, props);
    auto t2 = std::chrono::high_resolution_clock::now();
    std::cerr << "  Time: " << std::chrono::duration<double>(t2 - t1).count() << "s\n\n";

    // Step 3: Memory-map graph file
    std::cerr << "[3/5] Memory-mapping graph...\n";
    MappedFile graphFile;
    graphFile.open(basename + ".graph");
    std::cerr << "  Graph file size: " << graphFile.size() << " bytes\n\n";

    // Step 4: Parallel decode — build CSR (row_ptr + col_idx)
    std::cerr << "[4/5] Decoding graph into CSR...\n";
    auto t3 = std::chrono::high_resolution_clock::now();

    uint64_t numNodes = (uint64_t)props.nodes;
    uint64_t numEdges = (uint64_t)props.arcs;

    // row_ptr: numNodes entries (end offsets, implicit first 0 dropped per BGR spec)
    std::vector<uint64_t> rowPtr(numNodes);
    std::vector<uint64_t> colIdx(numEdges);

    // Pass 1: Compute degrees in parallel using offsets
    std::vector<uint64_t> degrees(numNodes);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int nthreads = omp_get_num_threads();
        int64_t chunkSize = ((int64_t)numNodes + nthreads - 1) / nthreads;
        int64_t startNode = (int64_t)tid * chunkSize;
        int64_t endNode = std::min(startNode + chunkSize, (int64_t)numNodes);

        for (int64_t v = startNode; v < endNode; v++) {
            InputBitStream ibs(graphFile.data(), graphFile.size());
            ibs.position(offsets[v]);
            degrees[v] = (uint64_t)readCoded(ibs, props.outdegreeCoding, props.zetaK);
        }
    }

    // Build row_ptr with prefix sum
    rowPtr[0] = degrees[0];
    for (uint64_t i = 1; i < numNodes; i++) {
        rowPtr[i] = rowPtr[i - 1] + degrees[i];
    }

    if (rowPtr[numNodes - 1] != numEdges) {
        std::cerr << "  WARNING: Edge count mismatch! rowPtr[last]=" << rowPtr[numNodes - 1]
                  << " vs arcs=" << numEdges << "\n";
        numEdges = rowPtr[numNodes - 1];
    }

    // Pass 2: Decode full successor lists in parallel
    std::atomic<int64_t> progressNodes(0);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int nthreads = omp_get_num_threads();
        int64_t chunkSize = ((int64_t)numNodes + nthreads - 1) / nthreads;
        int64_t startNode = (int64_t)tid * chunkSize;
        int64_t endNode = std::min(startNode + chunkSize, (int64_t)numNodes);

        if (startNode >= (int64_t)numNodes) {
            startNode = endNode = (int64_t)numNodes;
        }

        BVGraphDecoder decoder(props, graphFile.data(), graphFile.size(), offsets);
        decoder.warmupWindow(startNode);

        InputBitStream ibs(graphFile.data(), graphFile.size());

        for (int64_t v = startNode; v < endNode; v++) {
            ibs.position(offsets[v]);
            int64_t degree = decoder.decodeNodeWithWindow(v, ibs);

            int cyclicIdx = (int)(v % (props.windowSize + 1));
            const auto& succs = decoder.getWindow(cyclicIdx);

            uint64_t edgeStart = (v == 0) ? 0 : rowPtr[v - 1];
            for (int64_t j = 0; j < degree; j++) {
                colIdx[edgeStart + j] = (uint64_t)succs[j];
            }

            int64_t prog = progressNodes.fetch_add(1);
            if (prog % 100000 == 0) {
                #pragma omp critical
                std::cerr << "  Progress: " << prog << " / " << numNodes
                          << " (" << (100.0 * prog / numNodes) << "%)\r";
            }
        }
    }

    auto t4 = std::chrono::high_resolution_clock::now();
    std::cerr << "\n  Decode time: " << std::chrono::duration<double>(t4 - t3).count() << "s\n\n";

    // Step 5: Write BGR file
    std::cerr << "[5/5] Writing BGR file...\n";
    auto t5 = std::chrono::high_resolution_clock::now();

    BGRHeader h = makeBGRHeader(numNodes, numEdges);
    size_t totalSize = h.totalFileSize();
    std::cerr << "  File size: " << totalSize << " bytes\n";
    std::cerr << "  nodeU64=" << h.nodeU64 << " edgeU64=" << h.edgeU64 << "\n";

    int fd = open(outputFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        std::cerr << "Error: Cannot open output file: " << outputFile << "\n";
        return 1;
    }

    if (ftruncate(fd, totalSize) != 0) {
        close(fd);
        std::cerr << "Error: Failed to pre-allocate file\n";
        return 1;
    }

    // Write header
    uint8_t metaBuf[17];
    size_t metaSize = encodeBGRMeta(h, metaBuf);
    pwrite(fd, metaBuf, metaSize, 0);

    // Write row_ptr (parallel chunks) — element size = edgeBytes
    {
        size_t offset = h.rowPtrOffset();
        size_t elemBytes = h.edgeBytes();
        constexpr size_t CHUNK = 1024 * 1024;

        if (elemBytes == 4) {
            int nchunks = (int)((numNodes + CHUNK - 1) / CHUNK);
            #pragma omp parallel for schedule(dynamic)
            for (int c = 0; c < nchunks; c++) {
                uint64_t start = (uint64_t)c * CHUNK;
                uint64_t end = std::min(start + CHUNK, numNodes);
                size_t count = end - start;
                std::vector<uint32_t> buf(count);
                for (uint64_t i = 0; i < count; i++)
                    buf[i] = (uint32_t)rowPtr[start + i];
                pwrite(fd, buf.data(), count * 4, offset + start * 4);
            }
        } else {
            int nchunks = (int)((numNodes + CHUNK - 1) / CHUNK);
            #pragma omp parallel for schedule(dynamic)
            for (int c = 0; c < nchunks; c++) {
                uint64_t start = (uint64_t)c * CHUNK;
                uint64_t end = std::min(start + CHUNK, numNodes);
                size_t count = end - start;
                pwrite(fd, &rowPtr[start], count * 8, offset + start * 8);
            }
        }
    }

    // Write col_idx (parallel chunks) — element size = nodeBytes
    {
        size_t offset = h.colIdxOffset();
        size_t elemBytes = h.nodeBytes();
        constexpr size_t CHUNK = 1024 * 1024;

        if (elemBytes == 4) {
            int nchunks = (int)((numEdges + CHUNK - 1) / CHUNK);
            #pragma omp parallel for schedule(dynamic)
            for (int c = 0; c < nchunks; c++) {
                uint64_t start = (uint64_t)c * CHUNK;
                uint64_t end = std::min(start + CHUNK, numEdges);
                size_t count = end - start;
                std::vector<uint32_t> buf(count);
                for (uint64_t i = 0; i < count; i++)
                    buf[i] = (uint32_t)colIdx[start + i];
                pwrite(fd, buf.data(), count * 4, offset + start * 4);
            }
        } else {
            int nchunks = (int)((numEdges + CHUNK - 1) / CHUNK);
            #pragma omp parallel for schedule(dynamic)
            for (int c = 0; c < nchunks; c++) {
                uint64_t start = (uint64_t)c * CHUNK;
                uint64_t end = std::min(start + CHUNK, numEdges);
                size_t count = end - start;
                pwrite(fd, &colIdx[start], count * 8, offset + start * 8);
            }
        }
    }

    close(fd);

    auto t6 = std::chrono::high_resolution_clock::now();
    double totalTime = std::chrono::duration<double>(t6 - t0).count();
    double writeTime = std::chrono::duration<double>(t6 - t5).count();

    std::cerr << "  Write time: " << writeTime << "s\n\n";
    std::cerr << "=== Conversion Complete ===\n";
    std::cerr << "Total time: " << totalTime << "s\n";
    std::cerr << "Throughput: " << (numEdges / totalTime / 1e6) << " M edges/s\n";

    return 0;
}
