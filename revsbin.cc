#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "ips4o.hpp"
#include "toker.h"

using namespace hyperlink;

using NodeID = std::uint32_t;

int main(const int argc, const char *argv[]) {
    if (argc != 3) {
        std::cerr << "usage: ./revsbin <input.bin> <output.bin>\n";
        std::exit(1);
    }

    const std::string input_filename = argv[1];
    const std::string output_filename = argv[2];

    if (std::ifstream test_out(output_filename, std::ios::binary); test_out) {
        std::cerr << "error: output file already exists\n";
        std::exit(1);
    }
    
    std::ifstream in(input_filename, std::ios::binary);
    if (!in) {
        std::cerr << "error: could not open input file\n";
        std::exit(1);
    }

    in.seekg(0, std::ios::end);
    const std::uint64_t file_size = in.tellg();
    const std::uint64_t num_edges =
        file_size / sizeof(std::pair<NodeID, NodeID>);
    in.seekg(0, std::ios::beg);

    std::cout << "Preallocating " << file_size / 1024 / 1024 / 1024
              << " GB for " << num_edges << " edges ..." << std::endl;

    std::vector<std::pair<NodeID, NodeID>> edges(num_edges);

    std::cout << "Reading input file ..." << std::endl;
    in.read(reinterpret_cast<char *>(edges.data()), file_size);

    std::cout << "Reversing edges ..." << std::endl;
    for (std::pair<NodeID, NodeID> &edge : edges) {
        std::swap(edge.first, edge.second);
    }

    std::cout << "Sorting edges ..." << std::endl;
    ips4o::parallel::sort(edges.begin(), edges.end());

    std::cout << "Writing output file ..." << std::endl;
    std::ofstream out(output_filename, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "error: could not open output file\n";
        std::exit(1);
    }

    out.write(reinterpret_cast<const char *>(edges.data()),
              sizeof(std::pair<NodeID, NodeID>) * edges.size());

    std::cout << "Done." << std::endl;
}
