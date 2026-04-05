/*
 * bvgraph_gen_offsets.cpp — Generate .offsets file from .graph + .properties
 *
 * Sequentially scans the BVGraph bitstream, recording the bit position
 * of each node, and writes the gamma-coded deltas to a .offsets file.
 * No Java required.
 *
 * Usage: ./bvgraph_gen_offsets <basename>
 *   Creates <basename>.offsets from <basename>.graph + <basename>.properties
 */

#include "bvgraph_reader.h"
#include <chrono>

// ============================================================
// OutputBitStream — writes bits to a file
// ============================================================
class OutputBitStream {
public:
    OutputBitStream(const std::string& filename)
        : fout_(fopen(filename.c_str(), "wb")), buffer_(0), fill_(0), writtenBits_(0) {
        if (!fout_)
            throw std::runtime_error("Cannot open output file: " + filename);
    }

    ~OutputBitStream() { close(); }

    void close() {
        if (fout_) {
            flush();
            fclose(fout_);
            fout_ = nullptr;
        }
    }

    void writeBit(int bit) {
        buffer_ = (buffer_ << 1) | (bit & 1);
        fill_++;
        writtenBits_++;
        if (fill_ == 8) flushByte();
    }

    void writeBits(uint64_t value, int n) {
        for (int i = n - 1; i >= 0; i--)
            writeBit((int)(value >> i) & 1);
    }

    void writeUnary(int x) {
        for (int i = 0; i < x; i++) writeBit(0);
        writeBit(1);
    }

    void writeGamma(int64_t x) {
        if (x < 0) throw std::runtime_error("writeGamma: negative value");
        x++;
        int msb = 63 - __builtin_clzll((uint64_t)x);
        writeUnary(msb);
        if (msb > 0)
            writeBits((uint64_t)x & ((1ULL << msb) - 1), msb);
    }

    int64_t writtenBits() const { return writtenBits_; }

private:
    void flushByte() {
        fputc(buffer_, fout_);
        buffer_ = 0;
        fill_ = 0;
    }

    void flush() {
        if (fill_ > 0) {
            buffer_ <<= (8 - fill_);
            flushByte();
        }
    }

    FILE* fout_;
    uint8_t buffer_;
    int fill_;
    int64_t writtenBits_;
};

// Skip over a node's full encoding in the graph bitstream, advancing ibs
// Returns the outdegree of the node
static int skipNode(InputBitStream& ibs, const BVGraphProperties& props,
                    std::vector<std::vector<int>>& window,
                    std::vector<int>& outd,
                    int64_t x) {
    int cyclicBufferSize = props.windowSize + 1;
    int cyclicIdx = (int)(x % cyclicBufferSize);

    int d = readCoded(ibs, props.outdegreeCoding, props.zetaK);
    outd[cyclicIdx] = d;
    window[cyclicIdx].resize(d);

    if (d == 0) return 0;

    int ref = 0;
    if (props.windowSize > 0)
        ref = readCoded(ibs, props.referenceCoding, props.zetaK);

    int refIndex = (int)(((int64_t)x - ref + cyclicBufferSize) % cyclicBufferSize);
    int extraCount;
    std::vector<int> block;
    int refDegree = 0;

    if (ref > 0) {
        refDegree = outd[refIndex];
        int blockCount = readCoded(ibs, props.blockCountCoding, props.zetaK);
        if (blockCount > 0) {
            block.resize(blockCount);
            for (int i = 0; i < blockCount; i++)
                block[i] = readCoded(ibs, props.blockCoding, props.zetaK) + (i == 0 ? 0 : 1);
        }
        int copied = 0, total = 0;
        for (int i = 0; i < blockCount; i++) {
            total += block[i];
            if ((i & 1) == 0) copied += block[i];
        }
        if ((blockCount & 1) == 0) copied += refDegree - total;
        extraCount = d - copied;
    } else {
        extraCount = d;
    }

    // Read intervals
    int intervalCount = 0;
    std::vector<int64_t> left, len;
    if (extraCount > 0 && props.minIntervalLength != 0) {
        intervalCount = ibs.readGamma();
        if (intervalCount > 0) {
            left.resize(intervalCount);
            len.resize(intervalCount);
            int64_t prev;
            left[0] = prev = (int64_t)(nat2int(ibs.readLongGamma()) + x);
            len[0] = ibs.readGamma() + props.minIntervalLength;
            prev += len[0];
            extraCount -= (int)len[0];
            for (int i = 1; i < intervalCount; i++) {
                left[i] = prev = ibs.readGamma() + prev + 1;
                len[i] = ibs.readGamma() + props.minIntervalLength;
                prev += len[i];
                extraCount -= (int)len[i];
            }
        }
    }

    int residualCount = extraCount;

    // Read residuals
    std::vector<int64_t> residuals;
    if (residualCount > 0) {
        residuals.resize(residualCount);
        residuals[0] = x + nat2int(readLongCoded(ibs, props.residualCoding, props.zetaK));
        for (int i = 1; i < residualCount; i++)
            residuals[i] = residuals[i - 1] + readLongCoded(ibs, props.residualCoding, props.zetaK) + 1;
    }

    // Merge into window (needed for subsequent nodes' reference resolution)
    auto& succs = window[cyclicIdx];
    int pos = 0;

    int blockPos = 0, blockIdx = 0;
    bool blockCopy = true;
    int blockRemaining = 0;
    if (ref > 0) {
        if (!block.empty()) blockRemaining = block[0];
        else blockRemaining = refDegree;
    }
    std::vector<int>* refList = (ref > 0) ? &window[refIndex] : nullptr;

    auto nextBlock = [&]() -> int64_t {
        while (ref > 0 && blockPos < refDegree) {
            if (blockRemaining == 0) {
                blockIdx++;
                blockCopy = !blockCopy;
                if (blockIdx < (int)block.size()) blockRemaining = block[blockIdx];
                else if (blockCopy) blockRemaining = refDegree - blockPos;
                else return -1;
            }
            if (blockCopy && blockRemaining > 0) { blockRemaining--; return (*refList)[blockPos++]; }
            else if (!blockCopy && blockRemaining > 0) { blockRemaining--; blockPos++; }
            else return -1;
        }
        return -1;
    };

    int intervalIdx2 = 0;
    int64_t intervalPos2 = 0;
    auto nextInterval = [&]() -> int64_t {
        while (intervalIdx2 < intervalCount) {
            if (intervalPos2 < len[intervalIdx2])
                return left[intervalIdx2] + intervalPos2++;
            intervalIdx2++;
            intervalPos2 = 0;
        }
        return -1;
    };

    int residualIdx = 0;
    int64_t bVal = nextBlock();
    int64_t iVal = nextInterval();
    int64_t rVal = (residualIdx < residualCount) ? residuals[residualIdx] : -1;

    while (pos < d) {
        int64_t minVal = INT64_MAX;
        int src = -1;
        if (bVal >= 0 && bVal < minVal) { minVal = bVal; src = 0; }
        if (iVal >= 0 && iVal < minVal) { minVal = iVal; src = 1; }
        if (rVal >= 0 && rVal < minVal) { minVal = rVal; src = 2; }
        if (src < 0) break;
        succs[pos++] = (int)minVal;
        if (src == 0) bVal = nextBlock();
        else if (src == 1) iVal = nextInterval();
        else { residualIdx++; rVal = (residualIdx < residualCount) ? residuals[residualIdx] : -1; }
    }

    return d;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <basename>\n";
        std::cerr << "  Creates <basename>.offsets from <basename>.graph + <basename>.properties\n";
        return 1;
    }

    std::string basename = argv[1];
    auto t0 = std::chrono::high_resolution_clock::now();

    std::cerr << "=== BVGraph Offsets Generator (no Java) ===\n";
    std::cerr << "Basename: " << basename << "\n\n";

    // Parse properties
    BVGraphProperties props = parseProperties(basename);
    std::cerr << "Nodes: " << props.nodes << ", Arcs: " << props.arcs << "\n";

    // Memory-map graph file
    MappedFile graphFile;
    graphFile.open(basename + ".graph");
    std::cerr << "Graph file: " << graphFile.size() << " bytes\n";

    // Open output
    std::string outFile = basename + ".offsets";
    OutputBitStream obs(outFile);

    InputBitStream ibs(graphFile.data(), graphFile.size());

    int cyclicBufferSize = props.windowSize + 1;
    std::vector<std::vector<int>> window(cyclicBufferSize);
    std::vector<int> outd(cyclicBufferSize, 0);

    int64_t lastOffset = 0;

    for (int64_t v = 0; v < props.nodes; v++) {
        int64_t curOffset = ibs.position();
        obs.writeGamma(curOffset - lastOffset);
        lastOffset = curOffset;

        skipNode(ibs, props, window, outd, v);

        if (v % 100000 == 0)
            std::cerr << "  Progress: " << v << " / " << props.nodes
                      << " (" << (100.0 * v / props.nodes) << "%)\r";
    }

    // Write final offset delta (position after last node)
    obs.writeGamma(ibs.position() - lastOffset);
    obs.close();

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    std::cerr << "\nDone. Wrote " << outFile << " in " << elapsed << "s\n";
    std::cerr << "  " << props.nodes << " nodes, "
              << (props.nodes / elapsed / 1e6) << " M nodes/s\n";

    return 0;
}
