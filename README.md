# graph-format-converters

## .graph to .bin

### On Indus

### Input graphs need

- `./data/<graphName>.graph`
- `./data/<graphName>.properties`

### Steps
1. `export JAVA_HOME=/opt/jdk-17.0.12`
2. `export PATH=$JAVA_HOME/bin:$PATH`
3. `java --version`
4. `make clean`
5. `make WG2Bin args="data/<graphName> data"`

### Output files
- `<graphName>_edges.bin`
- `<graphName>_offsets.bin`
- `<graphName>.offsets`
- `<graphName>_props.txt`

## .bin to .egr

1. `g++ -O3 -fopenmp -Wall -Werror  -fsanitize=address,undefined   ReadWGBin.cpp -o ./data/Read.`
2. `cd data`
3. `./Read.o ./<graph_name>_props.txt ./<graph_name>.egr`