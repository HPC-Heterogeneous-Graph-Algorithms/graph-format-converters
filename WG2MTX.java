/*
 * WG2MTX.java — Convert WebGraph BVGraph to MTX (Matrix Market) format
 *
 * Uses the WebGraph Java library to iterate the graph and write edges
 * as a MatrixMarket coordinate pattern file.
 *
 * Usage: java -cp jlibs/*:. WG2MTX <basename> <output.mtx>
 */

import it.unimi.dsi.webgraph.*;
import java.io.*;

public class WG2MTX {
    public static void main(String[] args) throws Exception {
        if (args.length != 2) {
            System.err.println("Usage: java -cp jlibs/*:. WG2MTX <basename> <output.mtx>");
            System.exit(1);
        }

        String basename = args[0];
        String outputFile = args[1];

        long t0 = System.nanoTime();

        System.err.println("=== Java WebGraph to MTX Converter ===");
        System.err.println("Input:  " + basename);
        System.err.println("Output: " + outputFile);

        // Load graph
        ImmutableGraph graph = ImmutableGraph.loadMapped(basename);
        int numNodes = graph.numNodes();
        long numArcs = graph.numArcs();
        System.err.printf("Nodes: %,d%n", numNodes);
        System.err.printf("Arcs:  %,d%n", numArcs);

        // Write MTX
        try (PrintWriter pw = new PrintWriter(new BufferedWriter(
                new FileWriter(outputFile), 64 * 1024 * 1024))) {
            pw.println("%%MatrixMarket matrix coordinate pattern general");
            pw.println(numNodes + " " + numNodes + " " + numArcs);

            NodeIterator nodeIter = graph.nodeIterator();
            long edgeCount = 0;
            while (nodeIter.hasNext()) {
                int node = nodeIter.nextInt();
                int degree = nodeIter.outdegree();
                LazyIntIterator succs = nodeIter.successors();
                for (int j = 0; j < degree; j++) {
                    int dest = succs.nextInt();
                    // MTX is 1-indexed
                    pw.println((node + 1) + "\t" + (dest + 1));
                }
                edgeCount += degree;
                if (node % 500000 == 0) {
                    System.err.printf("  Progress: %,d / %,d nodes%n", node, numNodes);
                }
            }

            System.err.printf("Total edges written: %,d%n", edgeCount);
        }

        double elapsed = (System.nanoTime() - t0) / 1e9;
        System.err.printf("Time: %.2f s%n", elapsed);
    }
}
