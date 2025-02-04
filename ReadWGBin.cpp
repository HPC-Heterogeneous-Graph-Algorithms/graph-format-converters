#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdint>
// #include "ECLgraph.h"
#include <bits/stdc++.h>
#include <omp.h>
#include <fstream>
#include <vector>
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
struct ECLgraph
{
    long long int nodes;
    long long int edges;
    long long int *nindex;
    int *nlist;
    int *eweight;
};

void writeECLgraph(const ECLgraph g, const char *const fname)
{
    if ((g.nodes < 1) || (g.edges < 0))
    {
        fprintf(stderr, "ERROR: node or edge count too low\n\n");
        exit(-1);
    }
    long long int cnt;
    FILE *f = fopen(fname, "wb");
    if (f == NULL)
    {
        fprintf(stderr, "ERROR: could not open file %s\n\n", fname);
        exit(-1);
    }
    cnt = fwrite(&g.nodes, sizeof(g.nodes), 1, f);
    if (cnt != 1)
    {
        fprintf(stderr, "ERROR: failed to write nodes\n\n");
        exit(-1);
    }
    cnt = fwrite(&g.edges, sizeof(g.edges), 1, f);
    if (cnt != 1)
    {
        fprintf(stderr, "ERROR: failed to write edges\n\n");
        exit(-1);
    }

    cnt = fwrite(g.nindex, sizeof(g.nindex[0]), g.nodes + 1, f);
    if (cnt != g.nodes + 1)
    {
        fprintf(stderr, "ERROR: failed to write neighbor index list\n\n");
        exit(-1);
    }
    cnt = fwrite(g.nlist, sizeof(g.nlist[0]), g.edges, f);
    if (cnt != g.edges)
    {
        fprintf(stderr, "ERROR: failed to write neighbor list\n\n");
        exit(-1);
    }
    if (g.eweight != NULL)
    {
        cnt = fwrite(g.eweight, sizeof(g.eweight[0]), g.edges, f);
        if (cnt != g.edges)
        {
            fprintf(stderr, "ERROR: failed to write edge weights\n\n");
            exit(-1);
        }
    }
    fclose(f);
}

void freeECLgraph(ECLgraph &g)
{
    if (g.nindex != NULL)
        free(g.nindex);
    if (g.nlist != NULL)
        free(g.nlist);
    if (g.eweight != NULL)
        free(g.eweight);
    g.nindex = NULL;
    g.nlist = NULL;
    g.eweight = NULL;
}

void processGraphProps(const std::string &filename, long long int &verticesCount, long long int  &edgesCount, int &bytesPerVertexID, std::string &offsetsFile, std::string &edgesFile)
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


// Reads offsets from the binary file 'offsetsFile' into the preallocated array 'nindex'.
// The file is assumed to contain (verticesCount + 1) offsets of type int.
void readOffsets(const std::string &offsetsFile, long long verticesCount, long long *nindex) {
    // Open the file in binary mode.
    std::ifstream file(offsetsFile, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open offsets file " << offsetsFile << std::endl;
        std::exit(EXIT_FAILURE);
    }

    // Calculate the total number of bytes to read.
    int64_t bytesToRead = (verticesCount + 1) * sizeof(int64_t);

    // Read the data into the provided array.
    file.read(reinterpret_cast<char*>(nindex), bytesToRead);
    if (!file) {
        std::cerr << " Error: Only " << file.gcount() 
                  << " bytes could be read from " << offsetsFile 
                  << " (expected " << bytesToRead << " bytes)." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    file.close();
}


void readEdges(const std::string &filename, int *edges, long long *offsets, long long verticesCount, long long edgesCount, int bytesPerVertexID)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    std::vector<char> buffer(bytesPerVertexID);

    long long edgeIndex = 0;
    for ( long long i = 0; i < verticesCount; ++i)
    {
        long long start = offsets[i];
        long long end = offsets[i + 1];
        for (long long j = start; j < end; ++j)
        {
            file.read(buffer.data(), bytesPerVertexID);
            long long target = 0;
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

void readEdgesParallel(const std::string &filename, int *edges, long long *offsets, long long verticesCount, long long edgesCount, int bytesPerVertexID)
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

    long long fileSize = sb.st_size;

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
    for ( long long i = 0; i < verticesCount; ++i)
    {
         long long start = offsets[i];
         long long end = offsets[i + 1];

        // Optional: Uncomment the line below for debugging thread assignments
        // #pragma omp critical
        // std::cout << "Thread: " << omp_get_thread_num() << ", Vertex: " << i << ", Start: " << start << ", End: " << end << std::endl;

        for ( long long j = start; j < end; ++j)
        {
             long long pos = j * bytesPerVertexID;

            // Ensure we don't read beyond the file
            if (pos + bytesPerVertexID > fileSize)
            {
                std::cerr << "Error: Attempting to read beyond file size at edge index " << j << std::endl;
                edges[j] = 0; // Assign a default value or handle as needed
                continue;
            }

            int target = 0;

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

void printEdges(const long long *offsets, const int *edges, long long verticesCount)
{
    int count = 0;
    for ( long long i = 0; i < verticesCount; ++i)
    {
         long long start = offsets[i];
         long long end = offsets[i + 1];
        for ( long long j = start; j < end; ++j)
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
    g.nindex = (long long int *)malloc((verticesCount + 1)*sizeof(long long int ));
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
    long long int verticesCount,edgesCount;
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

    ECLgraph g;
    g.nodes = verticesCount;
    g.edges = edgesCount;
    g.nindex = (long long *)malloc((verticesCount + 1)* sizeof(g.nindex[0]));
    g.nlist = (int *)malloc(edgesCount * sizeof(g.nlist[0]));

    readOffsets(offsetsFile, verticesCount, g.nindex);

    // std::vector<uint64_t> offsets = readOffsets(offsetsFile, verticesCount);

    std::cout << "Offsets: ";
    int count = 0;
    for(int i=0;i<verticesCount+1;i++)
    {
        std::cout << g.nindex[i] << " ";
        count++;
        if(count > 10)
        {
            break;
        }
    }
    std::cout << std::endl;
    for(int i=verticesCount;i>verticesCount-10;i--)
    {
        std::cout << g.nindex[i] << " ";
    }
     std::cout << std::endl;

    // uint64_t *edges = new uint64_t[edgesCount];
//    uint64_t *edgesParallel = new uint64_t[edgesCount];
    readEdgesParallel(edgesFile,g.nlist, g.nindex, verticesCount, edgesCount, bytesPerVertexID);
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
    printEdges(g.nindex, g.nlist, verticesCount);
    for(long long i = edgesCount-1; i>=edgesCount-10;i--){
        std::cout << g.nlist[i] << " ";
    }
    std::cout<<"\n";

    // ECLgraph g = convertToECLgraph(offsets, edges, verticesCount, edgesCount);

    // std::cout << "Graph converted to ECL" << std:: endl;

    std::cout << "Number of nodes: " << g.nodes << std::endl;
    std::cout << "Number of edges: " << g.edges << std::endl;
    

    writeECLgraph(g, graph_output);
    
    std::cout << "Graph written to EGR" <<std::endl;

    sleep(10);

    freeECLgraph(g);
    return 0;
}
