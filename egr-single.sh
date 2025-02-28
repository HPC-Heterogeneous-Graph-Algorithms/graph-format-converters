#!/bin/bash
# Converter executable
CONVERTER="/home/lokesh/graph-format-converters/egr-single.o"

# Input and output directories
INDIR="/home/resources/Graphs/egr/sym/"
OUTDIR="/home/resources/Graphs/egr/single/"

# Create output directory if it doesn't exist
mkdir -p "$OUTDIR"

# Loop over all .egr files in the input directory
for infile in "$INDIR"/*.egr; do
    base=$(basename "$infile")
    outfile="$OUTDIR/$base"
    
    # Check if output file already exists
    if [ ! -f "$outfile" ]; then
        echo "Converting $infile -> $outfile"
        "$CONVERTER" "$infile" "$outfile"
    else
        echo "Skipping $infile as $outfile already exists."
    fi
done

echo "Conversion process completed in $OUTDIR"
