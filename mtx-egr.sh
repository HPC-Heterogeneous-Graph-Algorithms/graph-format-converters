#!/bin/bash
# Converter executable
CONVERTER="/home/lokesh/graph-format-converters/mtx-egr.o"

# Output directory for converted ECLgraph files
OUTDIR="/home/resources/Graphs/egr/sym/"

# Create the output directory if it doesn't exist
mkdir -p "$OUTDIR"

# List of MTX files to convert
files=(
    # "/home/graphwork/Graphs/LAW/webbase-2001.mtx"
#   "/home/resources/Graphs/LAW/sk-2005.mtx"
#   "/home/graphwork/Graphs/LAW/it-2004.mtx"
  "/home/resources/Graphs/GAP/GAP-kron.mtx"
#   "/home/graphwork/Graphs/GAP/GAP-urand.mtx"
#   "/home/graphwork/Graphs/GAP/GAP-twitter.mtx"
#   "/home/graphwork/Graphs/GAP/GAP-road.mtx"
#   "/home/resources/Graphs/SNAP/com-Friendster.mtx"
#   "/home/graphwork/Graphs/SNAP/twitter7.mtx"
#   "/home/graphwork/Graphs/Sybrandt/AGATHA_2015/AGATHA_2015.mtx"
#   "/home/graphwork/Graphs/DIMACS10/europe_osm.mtx"
#   "/home/graphwork/Graphs/DIMACS10/road_usa.mtx"
#   "/home/graphwork/Graphs/Sybrandt/MOLIERE_2016/MOLIERE_2016.mtx"
  
)

# Function to determine the weighted flag based on the file's base name.
# Unweighted graphs: webbase-2001, sk-2005, it-2004, com-Friendster, twitter7, AGATHA_2015 => flag 0;
# Otherwise, weighted => flag 1.
get_weighted_flag() {
    local base
    base=$(basename "$1" .mtx)
    if [[ "$base" =~ ^webbase-2001$ ]] || [[ "$base" =~ ^sk-2005$ ]] || [[ "$base" =~ ^it-2004$ ]] || \
       [[ "$base" =~ ^com-Friendster$ ]] || [[ "$base" =~ ^twitter7$ ]] || [[ "$base" =~ ^AGATHA_2015$ ]] || \
       [[ "$base" =~ ^europe_osm$ ]] || [[ "$base" =~ ^road_usa$ ]] ; then
        echo 0
    elif [[ "$base" =~ ^MOLIERE_2016$ ]] ; then
        echo 2
    else 
        echo 1
    fi
}

# Build an array of commands that use separate output files for each graph.
commands=()
for input in "${files[@]}"; do
    base=$(basename "$input" .mtx)
    output="$OUTDIR/$base.egr"
    weighted=$(get_weighted_flag "$input")
    commands+=("$CONVERTER \"$input\" \"$output\" \"$weighted\"")
done

# Print all the generated commands.
echo "The following conversion commands have been created:"
for cmd in "${commands[@]}"; do
    echo "$cmd"
done

echo -e "\nRunning each conversion sequentially..."
# Run each command one at a time.
for cmd in "${commands[@]}"; do
    echo "Running: $cmd"
    eval $cmd
done

echo "All conversions completed. Each output file is named as base_name.egr in $OUTDIR"
