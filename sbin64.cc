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

using NodeID = std::uint64_t;

int main(const int argc, const char *argv[]) {
    if (argc != 2) {
        std::cerr << "usage: ./revsbin <file.bin>\n";
        std::exit(1);
    }

    const std::string io_filename = argv[1];

    std::ifstream in(io_filename, std::ios::binary);
    if (!in) {
        std::cerr << "error: cannot read from " << io_filename << "\n";
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
    in.close();

    std::cout << "Reversing edges ..." << std::endl;
    for (std::pair<NodeID, NodeID> &edge : edges) {
        std::swap(edge.first, edge.second);
    }

    std::cout << "Sorting edges ..." << std::endl;
    ips4o::parallel::sort(edges.begin(), edges.end());

    std::cout << "Writing output file ..." << std::endl;
    std::ofstream out(io_filename, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::cerr << "error: cannot write to " << io_filename << "\n";
        std::exit(1);
    }

    out.write(reinterpret_cast<const char *>(edges.data()),
              sizeof(std::pair<NodeID, NodeID>) * edges.size());
    out.close();

    std::cout << "Done." << std::endl;
}
