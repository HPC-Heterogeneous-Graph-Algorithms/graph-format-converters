#pragma once
/*
 * bvgraph_reader.h — C++ BVGraph (WebGraph) reader
 *
 * A pure C++ implementation of the WebGraph BVGraph decompression format.
 * Reads .graph (bitstream), .properties (parameters), and .offsets (bit positions)
 * files from the LAW dataset format and decodes successor lists.
 *
 * References:
 *   - https://github.com/vigna/webgraph (Java source)
 *   - WebGraph BVGraph.java, CompressionFlags.java, InputBitStream (DSI utils)
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <unordered_map>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// ============================================================
// Compression coding constants (from CompressionFlags.java)
// ============================================================
enum CodingType {
    CODING_DELTA         = 1,
    CODING_GAMMA         = 2,
    CODING_GOLOMB        = 3,
    CODING_SKEWED_GOLOMB = 4,
    CODING_UNARY         = 5,
    CODING_ZETA          = 6,
    CODING_NIBBLE        = 7
};

static const char* CODING_NAMES[] = {
    "DEFAULT", "DELTA", "GAMMA", "GOLOMB", "SKEWED_GOLOMB", "UNARY", "ZETA", "NIBBLE"
};

// ============================================================
// InputBitStream — reads bits from a memory-mapped byte array
// ============================================================
class InputBitStream {
public:
    InputBitStream() : data_(nullptr), len_(0), pos_(0) {}

    InputBitStream(const uint8_t* data, size_t len)
        : data_(data), len_(len), pos_(0) {}

    void reset(const uint8_t* data, size_t len) {
        data_ = data;
        len_ = len;
        pos_ = 0;
    }

    // Get/set bit position
    int64_t position() const { return pos_; }
    void position(int64_t bitPos) { pos_ = bitPos; }

    // Read a single bit
    int readBit() {
        int byteIdx = (int)(pos_ >> 3);
        int bitIdx = 7 - (int)(pos_ & 7);
        pos_++;
        return (data_[byteIdx] >> bitIdx) & 1;
    }

    // Read n bits (up to 64)
    uint64_t readBits(int n) {
        if (n == 0) return 0;
        uint64_t result = 0;
        // Process bits - optimize for common cases
        while (n > 0) {
            int byteIdx = (int)(pos_ >> 3);
            int bitInByte = (int)(pos_ & 7);
            int bitsAvail = 8 - bitInByte;
            int bitsToRead = std::min(n, bitsAvail);

            uint8_t mask = (uint8_t)((1 << bitsToRead) - 1);
            int shift = bitsAvail - bitsToRead;
            uint64_t bits = (data_[byteIdx] >> shift) & mask;

            result = (result << bitsToRead) | bits;
            pos_ += bitsToRead;
            n -= bitsToRead;
        }
        return result;
    }

    // --------------------------------------------------------
    // Unary coding: count leading zeros, terminated by a 1
    // Encodes value x as x zeros followed by a 1
    // --------------------------------------------------------
    int readUnary() {
        int count = 0;
        while (readBit() == 0)
            count++;
        return count;
    }

    int64_t readLongUnary() {
        int64_t count = 0;
        while (readBit() == 0)
            count++;
        return count;
    }

    // --------------------------------------------------------
    // Gamma coding: unary(length) + binary(value)
    // length = floor(log2(x+1)), value = x+1 without MSB
    // --------------------------------------------------------
    int readGamma() {
        int len = readUnary();
        if (len == 0) return 0;
        return (int)(((uint64_t)1 << len) | readBits(len)) - 1;
    }

    int64_t readLongGamma() {
        int len = readUnary();
        if (len == 0) return 0;
        return (int64_t)(((uint64_t)1 << len) | readBits(len)) - 1;
    }

    // --------------------------------------------------------
    // Delta coding: gamma(length) + binary(value)
    // --------------------------------------------------------
    int readDelta() {
        int len = readGamma();
        if (len == 0) return 0;
        return (int)(((uint64_t)1 << len) | readBits(len)) - 1;
    }

    int64_t readLongDelta() {
        int64_t len = readLongGamma();
        if (len == 0) return 0;
        return (int64_t)(((uint64_t)1 << len) | readBits((int)len)) - 1;
    }

    // --------------------------------------------------------
    // Zeta_k coding: h = unary quotient, then remainder bits
    // If h > 0: (h-1) * k bits removed, remaining read
    // --------------------------------------------------------
    int readZeta(int k) {
        if (k == 3) return readZeta3();
        int h = readUnary();
        int left = 1 << (h * k);
        int m = (int)readBits(h * k + k - 1);
        if (m < left) {
            return m + left - 1;
        } else {
            return (m << 1) + readBit() - 1;
        }
    }

    int64_t readLongZeta(int k) {
        if (k == 3) return readLongZeta3();
        int h = readUnary();
        int64_t left = (int64_t)1 << (h * k);
        int64_t m = (int64_t)readBits(h * k + k - 1);
        if (m < left) {
            return m + left - 1;
        } else {
            return (m << 1) + readBit() - 1;
        }
    }

    // Optimized zeta_3
    int readZeta3() {
        int h = readUnary();
        int left = 1 << (h * 3);
        int m = (int)readBits(h * 3 + 2);
        if (m < left) {
            return m + left - 1;
        } else {
            return (m << 1) + readBit() - 1;
        }
    }

    int64_t readLongZeta3() {
        int h = readUnary();
        int64_t left = (int64_t)1 << (h * 3);
        int64_t m = (int64_t)readBits(h * 3 + 2);
        if (m < left) {
            return m + left - 1;
        } else {
            return (m << 1) + readBit() - 1;
        }
    }

    // --------------------------------------------------------
    // Nibble coding: 4-bit chunks with continuation
    // --------------------------------------------------------
    int readNibble() {
        int result = 0;
        int shift = 0;
        while (true) {
            int b = readBit();
            int nibble = (int)readBits(3);
            result |= (nibble << shift);
            shift += 3;
            if (b == 0) break;
        }
        return result;
    }

    int64_t readLongNibble() {
        int64_t result = 0;
        int shift = 0;
        while (true) {
            int b = readBit();
            int64_t nibble = (int64_t)readBits(3);
            result |= (nibble << shift);
            shift += 3;
            if (b == 0) break;
        }
        return result;
    }

private:
    const uint8_t* data_;
    size_t len_;
    int64_t pos_; // current bit position
};

// ============================================================
// nat2int / int2nat — signed zigzag encoding
// nat2int: 0→0, 1→-1, 2→1, 3→-2, 4→2, ...
// ============================================================
inline int64_t nat2int(int64_t x) {
    return (x >> 1) ^ -(x & 1);
}

inline int64_t int2nat(int64_t x) {
    return (x << 1) ^ (x >> 63);
}

// ============================================================
// BVGraphProperties — parsed from .properties file
// ============================================================
struct BVGraphProperties {
    int64_t nodes = 0;
    int64_t arcs = 0;
    int windowSize = 7;
    int maxRefCount = 3;
    int minIntervalLength = 4;
    int zetaK = 3;

    // Coding types for each component
    int outdegreeCoding = CODING_GAMMA;
    int blockCoding = CODING_GAMMA;
    int residualCoding = CODING_ZETA;
    int referenceCoding = CODING_UNARY;
    int blockCountCoding = CODING_GAMMA;
    int offsetCoding = CODING_GAMMA;

    std::string graphClass;
    int version = 0;
};

// ============================================================
// Parse compression flags string
// ============================================================
inline int parseCompressionFlags(const std::string& flagStr) {
    int flags = 0;
    std::istringstream iss(flagStr);
    std::string token;

    while (std::getline(iss, token, '|')) {
        // Trim whitespace
        size_t start = token.find_first_not_of(" \t\r\n");
        size_t end = token.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        token = token.substr(start, end - start + 1);
        if (token.empty()) continue;

        int value = 0;
        int shift = 0;

        if (token.find("OUTDEGREES_") == 0) {
            shift = 0;
            token = token.substr(11);
        } else if (token.find("BLOCKS_") == 0) {
            shift = 4;
            token = token.substr(7);
        } else if (token.find("RESIDUALS_") == 0) {
            shift = 8;
            token = token.substr(10);
        } else if (token.find("REFERENCES_") == 0) {
            shift = 12;
            token = token.substr(11);
        } else if (token.find("BLOCK_COUNT_") == 0) {
            shift = 16;
            token = token.substr(12);
        } else if (token.find("OFFSETS_") == 0) {
            shift = 20;
            token = token.substr(8);
        } else {
            std::cerr << "Warning: Unknown compression flag prefix: " << token << "\n";
            continue;
        }

        if (token == "DELTA") value = CODING_DELTA;
        else if (token == "GAMMA") value = CODING_GAMMA;
        else if (token == "GOLOMB") value = CODING_GOLOMB;
        else if (token == "SKEWED_GOLOMB") value = CODING_SKEWED_GOLOMB;
        else if (token == "UNARY") value = CODING_UNARY;
        else if (token == "ZETA") value = CODING_ZETA;
        else if (token == "NIBBLE") value = CODING_NIBBLE;
        else {
            std::cerr << "Warning: Unknown coding name: " << token << "\n";
            continue;
        }

        flags |= (value << shift);
    }
    return flags;
}

// ============================================================
// Parse .properties file
// ============================================================
inline BVGraphProperties parseProperties(const std::string& basename) {
    BVGraphProperties props;
    std::string filename = basename + ".properties";

    std::ifstream fin(filename);
    if (!fin.is_open())
        throw std::runtime_error("Cannot open properties file: " + filename);

    std::string line;
    std::unordered_map<std::string, std::string> kv;

    while (std::getline(fin, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || line[0] == '!') continue;

        // Find separator (= or :)
        size_t sep = line.find('=');
        if (sep == std::string::npos) sep = line.find(':');
        if (sep == std::string::npos) continue;

        std::string key = line.substr(0, sep);
        std::string val = line.substr(sep + 1);

        // Trim
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(val.begin());

        kv[key] = val;
    }
    fin.close();

    if (kv.count("nodes")) props.nodes = std::stoll(kv["nodes"]);
    if (kv.count("arcs")) props.arcs = std::stoll(kv["arcs"]);
    if (kv.count("windowsize")) props.windowSize = std::stoi(kv["windowsize"]);
    if (kv.count("maxrefcount")) props.maxRefCount = std::stoi(kv["maxrefcount"]);
    if (kv.count("minintervallength")) props.minIntervalLength = std::stoi(kv["minintervallength"]);
    if (kv.count("zetak")) props.zetaK = std::stoi(kv["zetak"]);
    if (kv.count("graphclass")) props.graphClass = kv["graphclass"];
    if (kv.count("version")) props.version = std::stoi(kv["version"]);

    if (kv.count("compressionflags") && !kv["compressionflags"].empty()) {
        int flags = parseCompressionFlags(kv["compressionflags"]);
        if ((flags & 0xF) != 0) props.outdegreeCoding = flags & 0xF;
        if (((flags >> 4) & 0xF) != 0) props.blockCoding = (flags >> 4) & 0xF;
        if (((flags >> 8) & 0xF) != 0) props.residualCoding = (flags >> 8) & 0xF;
        if (((flags >> 12) & 0xF) != 0) props.referenceCoding = (flags >> 12) & 0xF;
        if (((flags >> 16) & 0xF) != 0) props.blockCountCoding = (flags >> 16) & 0xF;
        if (((flags >> 20) & 0xF) != 0) props.offsetCoding = (flags >> 20) & 0xF;
    }

    return props;
}

// ============================================================
// Memory-mapped file wrapper
// ============================================================
class MappedFile {
public:
    MappedFile() : data_(nullptr), size_(0), fd_(-1) {}

    ~MappedFile() { close(); }

    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    void open(const std::string& filename) {
        fd_ = ::open(filename.c_str(), O_RDONLY);
        if (fd_ < 0)
            throw std::runtime_error("Cannot open file: " + filename);

        struct stat sb;
        if (fstat(fd_, &sb) < 0) {
            ::close(fd_);
            throw std::runtime_error("Cannot stat file: " + filename);
        }
        size_ = sb.st_size;

        if (size_ == 0) {
            data_ = nullptr;
            return;
        }

        data_ = (uint8_t*)mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (data_ == MAP_FAILED) {
            ::close(fd_);
            throw std::runtime_error("Cannot mmap file: " + filename);
        }

        // Advise sequential access
        madvise(data_, size_, MADV_SEQUENTIAL);
    }

    void close() {
        if (data_ && data_ != MAP_FAILED) {
            munmap(data_, size_);
            data_ = nullptr;
        }
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        size_ = 0;
    }

    const uint8_t* data() const { return data_; }
    size_t size() const { return size_; }

private:
    uint8_t* data_;
    size_t size_;
    int fd_;
};

// ============================================================
// Read coding value based on coding type
// ============================================================
inline int readCoded(InputBitStream& ibs, int coding, int zetaK = 3) {
    switch (coding) {
        case CODING_GAMMA: return ibs.readGamma();
        case CODING_DELTA: return ibs.readDelta();
        case CODING_UNARY: return ibs.readUnary();
        case CODING_ZETA:  return ibs.readZeta(zetaK);
        case CODING_NIBBLE: return ibs.readNibble();
        default:
            throw std::runtime_error("Unsupported coding type: " + std::to_string(coding));
    }
}

inline int64_t readLongCoded(InputBitStream& ibs, int coding, int zetaK = 3) {
    switch (coding) {
        case CODING_GAMMA: return ibs.readLongGamma();
        case CODING_DELTA: return ibs.readLongDelta();
        case CODING_UNARY: return ibs.readLongUnary();
        case CODING_ZETA:  return ibs.readLongZeta(zetaK);
        case CODING_NIBBLE: return ibs.readLongNibble();
        default:
            throw std::runtime_error("Unsupported coding type: " + std::to_string(coding));
    }
}

// ============================================================
// Load offsets file (.offsets)
// Returns vector of bit positions for each node
// ============================================================
inline std::vector<int64_t> loadOffsets(const std::string& basename,
                                        const BVGraphProperties& props) {
    std::string filename = basename + ".offsets";
    MappedFile offsetsFile;
    offsetsFile.open(filename);

    std::vector<int64_t> offsets(props.nodes + 1);
    InputBitStream ibs(offsetsFile.data(), offsetsFile.size());

    // First offset is always 0
    offsets[0] = 0;

    // Read and discard first encoded value (always 0)
    readLongCoded(ibs, props.offsetCoding, props.zetaK);

    // Read subsequent offset deltas
    for (int64_t i = 1; i <= props.nodes; i++) {
        int64_t delta = readLongCoded(ibs, props.offsetCoding, props.zetaK);
        offsets[i] = offsets[i - 1] + delta;
    }

    return offsets;
}

// ============================================================
// BVGraph decoder — decodes a single node's successor list
// ============================================================
class BVGraphDecoder {
public:
    BVGraphDecoder(const BVGraphProperties& props,
                   const uint8_t* graphData, size_t graphSize,
                   const std::vector<int64_t>& offsets)
        : props_(props), graphData_(graphData), graphSize_(graphSize),
          offsets_(offsets), cyclicBufferSize_(props.windowSize + 1) {
        window_.resize(cyclicBufferSize_);
        outd_.resize(cyclicBufferSize_, 0);
    }

    // Reset the window state (call before starting a new chunk)
    void resetWindow() {
        for (auto& w : window_) w.clear();
        std::fill(outd_.begin(), outd_.end(), 0);
    }

    // Pre-warm the window by decoding nodes [startNode - windowSize, startNode)
    // so that references from startNode can be resolved
    void warmupWindow(int64_t startNode) {
        resetWindow();
        int64_t from = std::max((int64_t)0, startNode - props_.windowSize);
        for (int64_t v = from; v < startNode; v++) {
            InputBitStream ibs(graphData_, graphSize_);
            ibs.position(offsets_[v]);
            decodeNodeWithWindow(v, ibs);
        }
    }

    // Decode a single node and update the window
    // Returns number of successors decoded
    int64_t decodeNodeWithWindow(int64_t x, InputBitStream& ibs) {
        int cyclicIdx = (int)(x % cyclicBufferSize_);

        // Read outdegree
        int d = readCoded(ibs, props_.outdegreeCoding, props_.zetaK);
        outd_[cyclicIdx] = d;
        window_[cyclicIdx].resize(d);

        if (d == 0) return 0;

        // Read reference
        int ref = 0;
        if (props_.windowSize > 0) {
            ref = readCoded(ibs, props_.referenceCoding, props_.zetaK);
        }

        int refIndex = (int)(((int64_t)x - ref + cyclicBufferSize_) % cyclicBufferSize_);

        int extraCount;
        std::vector<int>* refList = nullptr;
        int refDegree = 0;
        std::vector<int> block;

        if (ref > 0) {
            // We're referencing node (x - ref)
            refList = &window_[refIndex];
            refDegree = outd_[refIndex];

            // Read block count
            int blockCount = readCoded(ibs, props_.blockCountCoding, props_.zetaK);

            if (blockCount > 0) {
                block.resize(blockCount);
                for (int i = 0; i < blockCount; i++) {
                    block[i] = readCoded(ibs, props_.blockCoding, props_.zetaK) + (i == 0 ? 0 : 1);
                }
            }

            // Compute copied count
            int copied = 0, total = 0;
            for (int i = 0; i < blockCount; i++) {
                total += block[i];
                if ((i & 1) == 0) copied += block[i];
            }
            // If block count is even, remaining elements from ref are implicitly copied
            if ((blockCount & 1) == 0)
                copied += refDegree - total;

            extraCount = d - copied;
        } else {
            extraCount = d;
        }

        // Read intervals
        int intervalCount = 0;
        std::vector<int64_t> left, len;

        if (extraCount > 0 && props_.minIntervalLength != 0) {
            intervalCount = ibs.readGamma();
            if (intervalCount > 0) {
                left.resize(intervalCount);
                len.resize(intervalCount);

                int64_t prev;
                left[0] = prev = (int64_t)(nat2int(ibs.readLongGamma()) + x);
                len[0] = ibs.readGamma() + props_.minIntervalLength;
                prev += len[0];
                extraCount -= (int)len[0];

                for (int i = 1; i < intervalCount; i++) {
                    left[i] = prev = ibs.readGamma() + prev + 1;
                    len[i] = ibs.readGamma() + props_.minIntervalLength;
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
            residuals[0] = (int64_t)(x + nat2int(readLongCoded(ibs, props_.residualCoding, props_.zetaK)));
            for (int i = 1; i < residualCount; i++) {
                residuals[i] = residuals[i - 1] + readLongCoded(ibs, props_.residualCoding, props_.zetaK) + 1;
            }
        }

        // Merge all sources into the window entry for this node
        // The result must be in sorted order (which it is by construction)
        auto& succs = window_[cyclicIdx];
        int pos = 0;

        // Build iterators for three sources:
        // 1. Block-copied from reference
        // 2. Intervals
        // 3. Residuals

        // We merge them in sorted order
        int blockPos = 0;    // current position in reference list
        int blockIdx = 0;    // current block index
        bool blockCopy = true; // whether we're in a copy block (even) or skip block (odd)
        int blockRemaining = 0;
        if (ref > 0) {
            if (!block.empty()) {
                blockRemaining = block[0];
                blockCopy = true;
            } else {
                // No blocks but ref > 0: copy everything from reference
                blockRemaining = refDegree;
                blockCopy = true;
            }
        }

        int intervalIdx = 0;
        int64_t intervalPos = 0;
        int residualIdx = 0;

        // Get next from block source
        auto nextBlock = [&]() -> int64_t {
            while (ref > 0 && blockPos < refDegree) {
                if (blockRemaining == 0) {
                    blockIdx++;
                    blockCopy = !blockCopy;
                    if (blockIdx < (int)block.size()) {
                        blockRemaining = block[blockIdx];
                    } else {
                        // Past all blocks
                        if (blockCopy) {
                            // Even number of blocks exhausted, copy remaining
                            blockRemaining = refDegree - blockPos;
                        } else {
                            blockRemaining = 0;
                            return -1;
                        }
                    }
                }
                if (blockCopy && blockRemaining > 0) {
                    blockRemaining--;
                    return (*refList)[blockPos++];
                } else if (!blockCopy && blockRemaining > 0) {
                    blockRemaining--;
                    blockPos++;
                } else {
                    return -1;
                }
            }
            return -1;
        };

        // Get next from interval source
        auto nextInterval = [&]() -> int64_t {
            while (intervalIdx < intervalCount) {
                if (intervalPos < len[intervalIdx]) {
                    return left[intervalIdx] + intervalPos++;
                }
                intervalIdx++;
                intervalPos = 0;
            }
            return -1;
        };

        // Merge using three-way merge
        int64_t bVal = nextBlock();
        int64_t iVal = nextInterval();
        int64_t rVal = (residualIdx < residualCount) ? residuals[residualIdx] : -1;

        while (pos < d) {
            int64_t minVal = INT64_MAX;
            int src = -1; // 0=block, 1=interval, 2=residual

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

    // Decode a single node without window (using random access for references)
    // This recursively resolves references
    std::vector<int64_t> decodeNodeStandalone(int64_t x) {
        InputBitStream ibs(graphData_, graphSize_);
        ibs.position(offsets_[x]);

        // Read outdegree
        int d = readCoded(ibs, props_.outdegreeCoding, props_.zetaK);
        if (d == 0) return {};

        // Read reference
        int ref = 0;
        if (props_.windowSize > 0) {
            ref = readCoded(ibs, props_.referenceCoding, props_.zetaK);
        }

        // Get reference successors if needed
        std::vector<int64_t> refSuccs;
        int refDegree = 0;
        std::vector<int> block;

        int extraCount;

        if (ref > 0) {
            refSuccs = decodeNodeStandalone(x - ref);
            refDegree = (int)refSuccs.size();

            int blockCount = readCoded(ibs, props_.blockCountCoding, props_.zetaK);
            if (blockCount > 0) {
                block.resize(blockCount);
                for (int i = 0; i < blockCount; i++) {
                    block[i] = readCoded(ibs, props_.blockCoding, props_.zetaK) + (i == 0 ? 0 : 1);
                }
            }

            int copied = 0, total = 0;
            for (int i = 0; i < blockCount; i++) {
                total += block[i];
                if ((i & 1) == 0) copied += block[i];
            }
            if ((blockCount & 1) == 0)
                copied += refDegree - total;

            extraCount = d - copied;
        } else {
            extraCount = d;
        }

        // Read intervals
        int intervalCount = 0;
        std::vector<int64_t> left, len;

        if (extraCount > 0 && props_.minIntervalLength != 0) {
            intervalCount = ibs.readGamma();
            if (intervalCount > 0) {
                left.resize(intervalCount);
                len.resize(intervalCount);

                int64_t prev;
                left[0] = prev = (int64_t)(nat2int(ibs.readLongGamma()) + x);
                len[0] = ibs.readGamma() + props_.minIntervalLength;
                prev += len[0];
                extraCount -= (int)len[0];

                for (int i = 1; i < intervalCount; i++) {
                    left[i] = prev = ibs.readGamma() + prev + 1;
                    len[i] = ibs.readGamma() + props_.minIntervalLength;
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
            residuals[0] = (int64_t)(x + nat2int(readLongCoded(ibs, props_.residualCoding, props_.zetaK)));
            for (int i = 1; i < residualCount; i++) {
                residuals[i] = residuals[i - 1] + readLongCoded(ibs, props_.residualCoding, props_.zetaK) + 1;
            }
        }

        // Merge all three sources
        std::vector<int64_t> result(d);
        int pos = 0;

        // Block source from reference
        int blockPosR = 0, blockIdxR = 0;
        bool blockCopyR = true;
        int blockRemainingR = 0;
        if (ref > 0) {
            if (!block.empty()) {
                blockRemainingR = block[0];
            } else {
                blockRemainingR = refDegree;
            }
        }

        auto nextBlockR = [&]() -> int64_t {
            while (ref > 0 && blockPosR < refDegree) {
                if (blockRemainingR == 0) {
                    blockIdxR++;
                    blockCopyR = !blockCopyR;
                    if (blockIdxR < (int)block.size()) {
                        blockRemainingR = block[blockIdxR];
                    } else if (blockCopyR) {
                        blockRemainingR = refDegree - blockPosR;
                    } else {
                        return -1;
                    }
                }
                if (blockCopyR && blockRemainingR > 0) {
                    blockRemainingR--;
                    return refSuccs[blockPosR++];
                } else if (!blockCopyR && blockRemainingR > 0) {
                    blockRemainingR--;
                    blockPosR++;
                } else {
                    return -1;
                }
            }
            return -1;
        };

        int intervalIdxR = 0;
        int64_t intervalPosR = 0;
        auto nextIntervalR = [&]() -> int64_t {
            while (intervalIdxR < intervalCount) {
                if (intervalPosR < len[intervalIdxR]) {
                    return left[intervalIdxR] + intervalPosR++;
                }
                intervalIdxR++;
                intervalPosR = 0;
            }
            return -1;
        };

        int residualIdxR = 0;

        int64_t bV = nextBlockR();
        int64_t iV = nextIntervalR();
        int64_t rV = (residualIdxR < residualCount) ? residuals[residualIdxR] : -1;

        while (pos < d) {
            int64_t minV = INT64_MAX;
            int srcR = -1;

            if (bV >= 0 && bV < minV) { minV = bV; srcR = 0; }
            if (iV >= 0 && iV < minV) { minV = iV; srcR = 1; }
            if (rV >= 0 && rV < minV) { minV = rV; srcR = 2; }

            if (srcR < 0) break;

            result[pos++] = minV;

            if (srcR == 0) bV = nextBlockR();
            else if (srcR == 1) iV = nextIntervalR();
            else { residualIdxR++; rV = (residualIdxR < residualCount) ? residuals[residualIdxR] : -1; }
        }

        return result;
    }

    // Get the outdegree of a node (quick read, doesn't decode full successor list)
    int outdegree(int64_t x) {
        InputBitStream ibs(graphData_, graphSize_);
        ibs.position(offsets_[x]);
        return readCoded(ibs, props_.outdegreeCoding, props_.zetaK);
    }

    const BVGraphProperties& props() const { return props_; }

    // Access the window entry for reading successor lists after decoding
    const std::vector<int>& getWindow(int cyclicIdx) const { return window_[cyclicIdx]; }

private:
    const BVGraphProperties& props_;
    const uint8_t* graphData_;
    size_t graphSize_;
    const std::vector<int64_t>& offsets_;

    int cyclicBufferSize_;
    std::vector<std::vector<int>> window_;
    std::vector<int> outd_;
};

// end of bvgraph_reader.h
