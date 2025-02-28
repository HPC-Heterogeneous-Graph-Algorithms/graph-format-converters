#include <cstdio>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <egr_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE* fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    // Read number of nodes and edges (assumed to be stored as long long)
    long long nodes, edges;
    if (fread(&nodes, sizeof(long long), 1, fp) != 1) {
        fprintf(stderr, "Error reading number of nodes\n");
        fclose(fp);
        return EXIT_FAILURE;
    }
    if (fread(&edges, sizeof(long long), 1, fp) != 1) {
        fprintf(stderr, "Error reading number of edges\n");
        fclose(fp);
        return EXIT_FAILURE;
    }

    // Allocate memory for the CSR index array and neighbor list
    long long* nindex = new long long[nodes + 1];
    int* nlist = new int[edges];

    if (fread(nindex, sizeof(long long), nodes + 1, fp) != (size_t)(nodes + 1)) {
        fprintf(stderr, "Error reading CSR index array\n");
        delete[] nindex;
        fclose(fp);
        return EXIT_FAILURE;
    }

    if (fread(nlist, sizeof(int), edges, fp) != (size_t)edges) {
        fprintf(stderr, "Error reading neighbor list\n");
        delete[] nindex;
        delete[] nlist;
        fclose(fp);
        return EXIT_FAILURE;
    }

    fclose(fp);

    // Print the first 20 edges. In a CSR representation, for each vertex u,
    // the neighbors are stored in nlist from index nindex[u] to nindex[u+1]-1.
    int count = 0;
    for (long long u = 0; u < nodes && count < 20; u++) {
        for (long long idx = nindex[u]; idx < nindex[u+1] && count < 20; idx++) {
            printf("Edge %2d: %lld -> %d\n", count + 1, u, nlist[idx]);
            count++;
        }
    }

    delete[] nindex;
    delete[] nlist;

    return EXIT_SUCCESS;
}
