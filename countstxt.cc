#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#include "toker.h"

using namespace hyperlink;

using NodeID = std::uint64_t;

int main(const int argc, const char *argv[]) {
    if (argc < 2) {
        std::cerr << "usage: ./countstxtdups <input.txt>\n";
        std::exit(1);
    }

    const std::string input_filename = argv[1];

    if (std::ifstream in(input_filename); !in) {
        std::cerr << "error: could not open input file\n";
        std::exit(1);
    }

    MappedFileToker toker(input_filename);

    toker.SkipSpaces();

    NodeID prev_u = 0;
    NodeID prev_v = 0;

    std::uint64_t self_loops = 0;
    std::uint64_t multi_edges = 0;
    std::uint64_t lineno = 0;
    while (toker.ValidPosition()) {
        ++lineno;

        const NodeID u = static_cast<NodeID>(toker.ScanUInt());
        const NodeID v = static_cast<NodeID>(toker.ScanUInt());

        multi_edges += (prev_u == u && prev_v == v);
        self_loops += (u == v);

        if (prev_u > u || (prev_u == u && prev_v > v)) {
            std::cerr << "Error in line " << lineno
                      << ": input file is not sorted\n";
            std::cerr << "Previous edge: " << prev_u << "\t" << prev_v << "\n";
            std::cerr << "Current edge:  " << u << "\t" << v << "\n";
            std::exit(1);
        }

        prev_u = u;
        prev_v = v;
    }

    std::cout << "Edges:       " << lineno << "\n";
    std::cout << "Self-loops:  " << self_loops << "\n";
    std::cout << "Multi-edges: " << multi_edges << "\n";
    std::cout << "Remaining:   " << lineno - self_loops - multi_edges << "\n";
    std::cout << "Done." << std::endl;
}
