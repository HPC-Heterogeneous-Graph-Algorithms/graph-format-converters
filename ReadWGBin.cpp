#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdint>
#include "ECLgraph.h"
#include <bits/stdc++.h>
#include <omp.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <omp.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cassert>
#include <cstring>

using namespace std;

/*
Process this format of input

vertices-count:325557
edges-count:3216152
bytes-per-vertex-ID-in-edges-file:3
offsets-file:cnr-2000_offsets.bin
edges-file:cnr-2000_edges.bin
*/

void processGraphProps(const std::string &filename, uint64_t &verticesCount, uint64_t &edgesCount, int &bytesPerVertexID, std::string &offsetsFile, std::string &edgesFile)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string key;
        if (std::getline(iss, key, ':'))
        {
            std::string value;
            if (std::getline(iss, value))
            {
                if (key == "vertices-count")
                {
                    verticesCount = std::stoull(value);
                }
                else if (key == "edges-count")
                {
                    edgesCount = std::stoull(value);
                }
                else if (key == "bytes-per-vertex-ID-in-edges-file")
                {
                    bytesPerVertexID = std::stoi(value);
                }
                else if (key == "offsets-file")
                {
                    offsetsFile = value;
                }
                else if (key == "edges-file")
                {
                    edgesFile = value;
                }
            }
        }
    }

    file.close();
}
std::vector<uint64_t> readOffsets(const std::string &filename, uint64_t verticesCount)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Error opening file: " << filename << std::endl;
        return {};
    }

    std::vector<uint64_t> offsets(verticesCount + 1);
    file.read(reinterpret_cast<char *>(offsets.data()), (verticesCount + 1) * sizeof(uint64_t));

    if (!file)
    {
        std::cerr << "Error reading file: " << filename << std::endl;
    }

    file.close();
    return offsets;
}

void readEdges(const std::string &filename, uint64_t *edges, std::vector<uint64_t> &offsets, uint64_t verticesCount, uint64_t edgesCount, int bytesPerVertexID)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    std::vector<char> buffer(bytesPerVertexID);

    uint64_t edgeIndex = 0;
    for (uint64_t i = 0; i < verticesCount; ++i)
    {
        uint64_t start = offsets[i];
        uint64_t end = offsets[i + 1];
        for (uint64_t j = start; j < end; ++j)
        {
            file.read(buffer.data(), bytesPerVertexID);
            uint64_t target = 0;
            for (int k = 0; k < bytesPerVertexID; ++k)
            {
                // cout << target << " ";
                target |= (static_cast<unsigned char>(buffer[k]) << (k * 8));
                // cout << target << endl;
            }
            edges[edgeIndex] = target;
            edgeIndex++;
        }
    }

    file.close();
}

void readEdgesParallel(const std::string &filename, uint64_t *edges, std::vector<uint64_t> &offsets, uint64_t verticesCount, uint64_t edgesCount, int bytesPerVertexID)
{
    // Open the file once using POSIX open
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1)
    {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    // Get the file size
    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        std::cerr << "Error getting file size: " << filename << std::endl;
        close(fd);
        return;
    }

    size_t fileSize = sb.st_size;

    // Memory-map the file
    char *fileData = static_cast<char *>(mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fd, 0));
    if (fileData == MAP_FAILED)
    {
        std::cerr << "Error memory-mapping the file: " << filename << std::endl;
        close(fd);
        return;
    }

    // Close the file descriptor as it's no longer needed after mmap
    close(fd);

    // Determine if the system is little endian
    bool isLittleEndian = true;
    uint16_t num = 1;
    if (*(reinterpret_cast<char *>(&num)) != 1)
        isLittleEndian = false;

    // Precompute shifts based on endianness
    std::vector<int> shifts(bytesPerVertexID);
    if (isLittleEndian)
    {
        for (int k = 0; k < bytesPerVertexID; ++k)
            shifts[k] = k * 8;
    }
    else
    {
        for (int k = 0; k < bytesPerVertexID; ++k)
            shifts[k] = (bytesPerVertexID - 1 - k) * 8;
    }

// Parallel processing using OpenMP
#pragma omp parallel for schedule(dynamic)
    for (uint64_t i = 0; i < verticesCount; ++i)
    {
        uint64_t start = offsets[i];
        uint64_t end = offsets[i + 1];

        // Optional: Uncomment the line below for debugging thread assignments
        // #pragma omp critical
        // std::cout << "Thread: " << omp_get_thread_num() << ", Vertex: " << i << ", Start: " << start << ", End: " << end << std::endl;

        for (uint64_t j = start; j < end; ++j)
        {
            uint64_t pos = j * bytesPerVertexID;

            // Ensure we don't read beyond the file
            if (pos + bytesPerVertexID > fileSize)
            {
                std::cerr << "Error: Attempting to read beyond file size at edge index " << j << std::endl;
                edges[j] = 0; // Assign a default value or handle as needed
                continue;
            }

            uint64_t target = 0;

            // Efficiently convert bytes to uint64_t using precomputed shifts
            for (int k = 0; k < bytesPerVertexID; ++k)
            {
                target |= (static_cast<unsigned char>(fileData[pos + k]) << shifts[k]);
            }

            edges[j] = target;
        }
    }

    // Unmap the file
    if (munmap(fileData, fileSize) == -1)
    {
        std::cerr << "Error unmapping the file: " << filename << std::endl;
        return;
    }
}

void printEdges(const std::vector<uint64_t> &offsets, const uint64_t *edges, uint64_t verticesCount)
{
    int count = 0;
    for (uint64_t i = 0; i < verticesCount; ++i)
    {
        uint64_t start = offsets[i];
        uint64_t end = offsets[i + 1];
        for (uint64_t j = start; j < end; ++j)
        {
            std::cout << "Source: " << i << ", Target: " << edges[j] << std::endl;
            count++;
            if (count > 10)
            {
                break;
            }
        }
        if (count > 10)
        {
            break;
        }
    }
}

// Function to convert the input graph into an ECLgraph
ECLgraph convertToECLgraph(const std::vector<uint64_t> &offsets, const uint64_t *edges, uint64_t verticesCount, uint64_t edgesCount)
{
    ECLgraph g;
    g.nodes = verticesCount;
    g.edges = edgesCount;
    g.nindex = (int *)calloc(verticesCount + 1, sizeof(int));
    // g.nindex.resize(verticesCount + 1);
    g.nlist = (int *)malloc(edgesCount * sizeof(int));
    // g.nlist.resize(edgesCount);
    g.eweight = NULL;

#pragma omp parallel for
    for (uint64_t i = 0; i < verticesCount; ++i)
    {
        g.nindex[i] = offsets[i];
    }

    g.nindex[verticesCount] = edgesCount;

#pragma omp parallel for
    for (uint64_t i = 0; i < edgesCount; ++i)
    {
        g.nlist[i] = edges[i];
    }

    return g;
}

int main(int argc, char *argv[])
{
    uint64_t verticesCount, edgesCount;
    int bytesPerVertexID;
    std::string offsetsFile, edgesFile;

    string props_filename = argv[1];
    const char *graph_output = argv[2];

    processGraphProps(props_filename, verticesCount, edgesCount, bytesPerVertexID, offsetsFile, edgesFile);

    std::cout << "Vertices Count: " << verticesCount << std::endl;
    std::cout << "Edges Count: " << edgesCount << std::endl;
    std::cout << "Bytes Per Vertex ID: " << bytesPerVertexID << std::endl;
    std::cout << "Offsets File: " << offsetsFile << std::endl;
    std::cout << "Edges File: " << edgesFile << std::endl;

    std::vector<uint64_t> offsets = readOffsets(offsetsFile, verticesCount);

    std::cout << "Offsets: ";
    int count = 0;
    for (const auto &offset : offsets)
    {
        std::cout << offset << " ";
        count++;
        if (count > 10)
        {
            break;
        }
    }
    std::cout << std::endl;

    uint64_t *edges = new uint64_t[edgesCount];
//    uint64_t *edgesParallel = new uint64_t[edgesCount];
    readEdgesParallel(edgesFile, edges, offsets, verticesCount, edgesCount, bytesPerVertexID);
//    readEdges(edgesFile, edgesParallel, offsets, verticesCount, edgesCount, bytesPerVertexID);

    std::cout << "Edges Alloted" << std::endl;
// Compare the two methods
/*
#pragma omp parallel for
    for (uint64_t i = 0; i < edgesCount; ++i)
    {
        if (edges[i] != edgesParallel[i])
        {
            std::cerr << i << " " << edges[i] << " " << edgesParallel[i] << std::endl;
            std::cerr << "Error: Parallel and non-parallel methods do not match" << std::endl;
        }
    }
*/
    printEdges(offsets, edges, verticesCount);

    ECLgraph g = convertToECLgraph(offsets, edges, verticesCount, edgesCount);

    std::cout << "Graph converted to ECL" << std:: endl;

    writeECLgraph(g, graph_output);
    
    std::cout << "Graph written to EGR" <<std::endl;

    freeECLgraph(g);
    return 0;
}
