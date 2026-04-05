/*
 * WG2BGR.java — Convert WebGraph BVGraph directly to BGR format
 *
 * Uses the WebGraph Java library to iterate the graph, build CSR in memory,
 * and write the BGR binary format. No intermediate files needed.
 *
 * Usage: java -cp jlibs/*:. WG2BGR <basename> <output.bgr>
 */

import it.unimi.dsi.webgraph.*;
import java.io.*;
import java.nio.*;
import java.nio.channels.*;

public class WG2BGR {
    public static void main(String[] args) throws Exception {
        if (args.length != 2) {
            System.err.println("Usage: java -cp jlibs/*:. WG2BGR <basename> <output.bgr>");
            System.exit(1);
        }

        String basename = args[0];
        String outputFile = args[1];

        long t0 = System.nanoTime();

        System.err.println("=== Java WebGraph to BGR Converter ===");
        System.err.println("Input:  " + basename);
        System.err.println("Output: " + outputFile);

        // Load graph
        ImmutableGraph graph = ImmutableGraph.loadMapped(basename);
        long numNodes = graph.numNodes();
        long numArcs = graph.numArcs();
        System.err.printf("Nodes: %,d%n", numNodes);
        System.err.printf("Arcs:  %,d%n", numArcs);

        // Determine sizes
        boolean nodeU64 = (numNodes > 0xFFFFFFFFL);
        boolean edgeU64 = (numArcs > 0xFFFFFFFFL);
        int nodeBytes = nodeU64 ? 8 : 4;
        int edgeBytes = edgeU64 ? 8 : 4;
        byte flags = (byte) ((nodeU64 ? 1 : 0) | (edgeU64 ? 2 : 0));

        System.err.printf("nodeU64=%b, edgeU64=%b, flags=0x%02X%n", nodeU64, edgeU64, flags);

        // Build CSR: row_ptr (numNodes entries) and col_idx (numArcs entries)
        System.err.println("Building CSR...");
        long[] rowPtr = new long[(int) numNodes];
        int[] colIdx = (numArcs <= Integer.MAX_VALUE) ? new int[(int) numArcs] : null;
        long[] colIdxLong = (colIdx == null) ? new long[(int) numArcs] : null;

        NodeIterator nodeIter = graph.nodeIterator();
        long edgePos = 0;
        while (nodeIter.hasNext()) {
            int node = nodeIter.nextInt();
            int degree = nodeIter.outdegree();
            LazyIntIterator succs = nodeIter.successors();
            for (int j = 0; j < degree; j++) {
                int dest = succs.nextInt();
                if (colIdx != null) colIdx[(int) edgePos] = dest;
                else colIdxLong[(int) edgePos] = dest;
                edgePos++;
            }
            rowPtr[node] = edgePos;  // end offset (BGR drops implicit row_ptr[0]=0)

            if (node % 500000 == 0) {
                System.err.printf("  Progress: %,d / %,d nodes%n", node, numNodes);
            }
        }

        System.err.printf("CSR built: %,d edges%n", edgePos);

        // Write BGR file
        System.err.println("Writing BGR...");
        try (RandomAccessFile raf = new RandomAccessFile(outputFile, "rw")) {
            long totalSize = 1 + nodeBytes + edgeBytes
                    + (long) edgeBytes * numNodes     // row_ptr
                    + (long) nodeBytes * numArcs;     // col_idx
            raf.setLength(totalSize);
            FileChannel fc = raf.getChannel();

            ByteBuffer headerBuf = ByteBuffer.allocate(17).order(ByteOrder.LITTLE_ENDIAN);
            headerBuf.put(flags);
            if (nodeU64) headerBuf.putLong(numNodes);
            else headerBuf.putInt((int) numNodes);
            if (edgeU64) headerBuf.putLong(numArcs);
            else headerBuf.putInt((int) numArcs);
            headerBuf.flip();
            fc.write(headerBuf, 0);

            long rowPtrOffset = 1 + nodeBytes + edgeBytes;
            long colIdxOffset = rowPtrOffset + (long) edgeBytes * numNodes;

            // Write row_ptr
            int chunkSize = 1024 * 1024;
            for (int start = 0; start < numNodes; start += chunkSize) {
                int end = (int) Math.min(start + chunkSize, numNodes);
                int count = end - start;
                ByteBuffer buf = ByteBuffer.allocate(count * edgeBytes).order(ByteOrder.LITTLE_ENDIAN);
                for (int i = start; i < end; i++) {
                    if (edgeU64) buf.putLong(rowPtr[i]);
                    else buf.putInt((int) rowPtr[i]);
                }
                buf.flip();
                fc.write(buf, rowPtrOffset + (long) start * edgeBytes);
            }

            // Write col_idx
            for (long start = 0; start < numArcs; start += chunkSize) {
                int end = (int) Math.min(start + chunkSize, numArcs);
                int count = end - (int) start;
                ByteBuffer buf = ByteBuffer.allocate(count * nodeBytes).order(ByteOrder.LITTLE_ENDIAN);
                for (int i = (int) start; i < end; i++) {
                    int val = (colIdx != null) ? colIdx[i] : (int) colIdxLong[i];
                    if (nodeU64) buf.putLong(val);
                    else buf.putInt(val);
                }
                buf.flip();
                fc.write(buf, colIdxOffset + start * nodeBytes);
            }

            fc.close();
        }

        double elapsed = (System.nanoTime() - t0) / 1e9;
        System.err.printf("Done. File: %,d bytes, Time: %.2f s%n",
                new File(outputFile).length(), elapsed);
    }
}
