#include<bits/stdc++.h>
#include <execution>
#include <iostream>
#include <vector>
#include <algorithm>
using namespace std;


void readECLgraph(const char* input_file, const char* output_file) {
    ifstream inFile(input_file, ios::binary);
    if (!inFile) {
        throw runtime_error("Error opening file: ");
    }

    // Reading sizes
    vector<long> vertices;
    vector<long> edges;
    size_t size;
    inFile.read(reinterpret_cast<char*>(&size), sizeof(size));
    vertices.resize(size);
    inFile.read(reinterpret_cast<char*>(&size), sizeof(size));
    edges.resize(size);

    // Reading data
    inFile.read(reinterpret_cast<char*>(vertices.data()), vertices.size() * sizeof(long));
    // inFile.read(reinterpret_cast<char*>(edges.data()), edges.size() * sizeof(int));
    vector<int> partial(128);
    int u;
    long diff, temp_size;
    long ct = 0;
    for(long i=0;i<vertices.size()-1;i++){
        u = i;
        for(long j = vertices[i]; j < vertices[i+1]; j+=128){
            diff = vertices[i+1]-j;
            temp_size = min(diff,(long)128);
            inFile.read(reinterpret_cast<char*>(partial.data()),temp_size * sizeof(int));
            for(int k = 0; k<temp_size;k++){
                edges[ct++]= (static_cast<long>(min(u,partial[k])) << 32) | max(u,partial[k]);
            }
        }
    }
    assert(ct == edges.size());
    sort(execution::par,edges.begin(),edges.end());

    long numVert = vertices.size() - 1;
    long numEdges = edges.size();

    ofstream fout(output_file, ios::binary);
    if (!fout.is_open()) {
        cerr << "ERROR: could not open output file " << output_file << endl;
        exit(-1);
    }

    fout.write(reinterpret_cast<const char*>(&numVert), sizeof(long));
    fout.write(reinterpret_cast<const char*>(&numEdges), sizeof(long));
    int src;
    int dst;
    long finalEdges = 0; // rename to avoid conflict
    for (long i = 0; i < numEdges; ++i) {

        if(i > 0 && edges[i] == edges[i - 1])
            continue;
        src = edges[i] >> 32;
        dst = edges[i] & 0xFFFFFFFF;

        if (src >= numVert || dst >= numVert) {
            cerr << "ERROR: invalid edge data "<< i << " " <<src<<" "<<dst << endl;
            exit(-1);
        }

        if(src == dst)
            continue;

        if (src > int(-1) || dst > int(-1)) {
            cerr << "ERROR: node id is larger than uint32_t "<< i << " " <<src<<" "<<dst << endl;
            exit(-1);
        }

        fout.write(reinterpret_cast<const char*>(&src), sizeof(uint32_t));
        fout.write(reinterpret_cast<const char*>(&dst), sizeof(uint32_t));
        finalEdges++;
    }
    cout << "Trimmed number of edges: " << finalEdges << endl;

    inFile.close();
    fout.close();
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "USAGE: " << argv[0] << " input_file_name output_file_name" << endl;
        return -1;
    }

    // Call the function to read the .mtx file and write the edge list as uint64_t
    readECLgraph(argv[1], argv[2]);

    return 0;
}
