#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>
#include <utility>
#include <algorithm>
#include "ECLgraph.h"
#include <bits/stdc++.h>
using namespace std;

int main(int argc, char* argv[]) {
  printf("MTX to ECL Graph Converter (Symmetrized, ignoring weights)\n");
  printf("Copyright 2019 Texas State University\n");

  if (argc != 4) {
    fprintf(stderr, "USAGE: %s input_file_name output_file_name <has weights>\n\n", argv[0]);
    exit(-1);
  }

  FILE* fin = fopen(argv[1], "rt");
  if (fin == NULL) {
    fprintf(stderr, "ERROR: could not open input file %s\n\n", argv[1]);
    exit(-1);
  }

  // Read and ignore header and comment lines.
  char line[256];
  char* ptr = line;
  size_t linesize = 256;
  if (getline(&ptr, &linesize, fin) < 0) {
    fprintf(stderr, "ERROR: could not read first line\n\n");
    exit(-1);
  }
  // Skip comment lines (starting with '%')
  while (fgets(line, sizeof(line), fin)) {
    if (line[0] != '%') break;
  }
  // Read graph size line: nodes, dummy, edges
  long long nodes, dummy, edges;
  if (sscanf(line, "%lld %lld %lld", &nodes, &dummy, &edges) != 3 || nodes < 1 || edges < 0 || nodes != dummy) {
    fprintf(stderr, "ERROR: failed to parse first data line\n\n");
    exit(-1);
  }

  // Flag to indicate if the file has weight information.
  int hasWeights = atoi(argv[3]);

  // We'll store edges in a vector. For each edge (u,v) we also add (v,u) to ensure symmetry.
  std::vector<std::pair<int, int>> edgeList;
  int src, dst, dummyWeight;
  int readCount = 0;
  
  if (hasWeights==0) {
    while (fscanf(fin, "%d %d", &src, &dst) == 2) {
      readCount++;
      if (src < 1 || src > nodes || dst < 1 || dst > nodes) {
        fprintf(stderr, "ERROR: vertex out of range\n\n");
        exit(-1);
      }
      // Convert from 1-indexed to 0-indexed.
      int u = src - 1, v = dst - 1;
      edgeList.push_back(std::make_pair(u, v));
      if (u != v)  // Avoid duplicate for self-loops.
        edgeList.push_back(std::make_pair(v, u));
    }
  } 
  else if (hasWeights==1) {
    while (fscanf(fin, "%d %d %d", &src, &dst, &dummyWeight) == 3) {
      readCount++;
      if (src < 1 || src > nodes || dst < 1 || dst > nodes) {
        fprintf(stderr, "ERROR: vertex out of range\n\n");
        exit(-1);
      }
      int u = src - 1, v = dst - 1;
      edgeList.push_back(std::make_pair(u, v));
      if (u != v)
        edgeList.push_back(std::make_pair(v, u));
    }
  }
  else if (hasWeights==2) {
    string dummy_weight;
    while (fscanf(fin, "%d %d %s", &src, &dst, &dummy_weight) == 3) {
      readCount++;
      if (src < 1 || src > nodes || dst < 1 || dst > nodes) {
        fprintf(stderr, "ERROR: vertex out of range\n\n");
        exit(-1);
      }
      int u = src - 1, v = dst - 1;
      edgeList.push_back(std::make_pair(u, v));
      if (u != v)
        edgeList.push_back(std::make_pair(v, u));
    }
  }
  fclose(fin);

  // Note: the original file edge count is for one direction; we have potentially doubled that.
  // Sort the edges and remove duplicates.
  std::sort(edgeList.begin(), edgeList.end());
  edgeList.erase(std::unique(edgeList.begin(), edgeList.end()), edgeList.end());
  
  long long finalEdges = edgeList.size();
  printf("%s\t#name\n", argv[1]);
  printf("%lld\t#nodes\n", nodes);
  printf("%lld\t#edges (after symmetrization)\n", finalEdges);
  printf("no\t#weights\n");

  // Build the CSR representation.
  ECLgraph g;
  g.nodes = nodes;
  g.edges = finalEdges;
  g.nindex = (long long*)calloc(nodes + 1, sizeof(long long));
  g.nlist = (int*)malloc(finalEdges * sizeof(int));
  g.eweight = NULL;  // We ignore weights

  if (g.nindex == NULL || g.nlist == NULL) {
    fprintf(stderr, "ERROR: memory allocation failed\n\n");
    exit(-1);
  }

  // Count the number of edges per node.
  for (size_t i = 0; i < edgeList.size(); i++) {
    int u = edgeList[i].first;
    g.nindex[u + 1]++;
  }
  // Convert counts to cumulative indices.
  for (int i = 0; i < nodes; i++) {
    g.nindex[i + 1] += g.nindex[i];
  }

  // Temporary copy of nindex to use as current insertion index.
  std::vector<long long> tempIndex(g.nindex, g.nindex + nodes + 1);

  // Insert the destination vertices into nlist.
  for (size_t i = 0; i < edgeList.size(); i++) {
    int u = edgeList[i].first;
    int v = edgeList[i].second;
    long long pos = tempIndex[u]++;
    g.nlist[pos] = v;
  }

  // Optionally, one can sort the neighbor lists per node here if needed.

  // Write out the ECLgraph in binary or text format using the provided function.
  writeECLgraph(g, argv[2]);

  // Free allocated memory.
  freeECLgraph(g);
  return 0;
}
