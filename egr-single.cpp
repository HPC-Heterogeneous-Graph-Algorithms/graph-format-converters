#include <cstdio>
#include <cstdlib>
#include "ECLgraph.h"

int main(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s input_egr output_egr\n", argv[0]);
    exit(1);
  }

  // Read the symmetric EGR graph.
  ECLgraph g = readECLgraph(argv[1]);
  
  // Count the number of edges that satisfy v > u.
  long long newEdges = 0;
  for (long long u = 0; u < g.nodes; u++) {
    for (long long j = g.nindex[u]; j < g.nindex[u+1]; j++) {
      int v = g.nlist[j];
      if (v > u) {
        newEdges++;
      }
    }
  }
  
  // Allocate memory for the unsymmetric graph.
  ECLgraph unsym;
  unsym.nodes = g.nodes;
  unsym.edges = newEdges;
  unsym.nindex = (long long*) malloc((unsym.nodes + 1) * sizeof(long long));
  unsym.nlist  = (int*) malloc(newEdges * sizeof(int));
  unsym.eweight = NULL;  // We ignore weights

  if (unsym.nindex == NULL || unsym.nlist == NULL) {
    fprintf(stderr, "ERROR: memory allocation failed\n");
    exit(1);
  }

  // Build the new CSR representation.
  unsym.nindex[0] = 0;
  long long edgeCounter = 0;
  for (long long u = 0; u < unsym.nodes; u++) {
    // For each vertex u, only add edges (u,v) if v > u (ignoring self-loops).
    for (long long j = g.nindex[u]; j < g.nindex[u+1]; j++) {
      int v = g.nlist[j];
      if (v > u) {
        unsym.nlist[edgeCounter++] = v;
      }
    }
    unsym.nindex[u+1] = edgeCounter;
  }

  // Write out the unsymmetric graph.
  writeECLgraph(unsym, argv[2]);

  // Clean up memory.
  freeECLgraph(unsym);
  freeECLgraph(g);

  return 0;
}
