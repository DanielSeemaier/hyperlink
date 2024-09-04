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
    if (argc < 2) {
        std::cerr << "usage: ./sbin64 <files>\n";
        std::exit(1);
    }

    for (std::size_t i = 1; i < argc; ++i) {
        const std::string io_filename = argv[i];

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

        std::cout << io_filename << ": preallocating "
                  << file_size / 1024 / 1024 / 1024 << " GB for " << num_edges
                  << " edges ..." << std::endl;

        std::vector<std::pair<NodeID, NodeID>> edges(num_edges);

        std::cout << io_filename << ": reading input file ..." << std::endl;
        in.read(reinterpret_cast<char *>(edges.data()), file_size);
        in.close();

        std::cout << io_filename << ": reversing edges ..." << std::endl;
        for (std::pair<NodeID, NodeID> &edge : edges) {
            std::swap(edge.first, edge.second);
        }

        std::cout << io_filename << ": sorting edges ..." << std::endl;
        ips4o::parallel::sort(edges.begin(), edges.end());

        std::cout << io_filename << ": writing output file ..." << std::endl;
        std::ofstream out(io_filename, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::cerr << "error: cannot write to " << io_filename << "\n";
            std::exit(1);
        }

        out.write(reinterpret_cast<const char *>(edges.data()),
                  sizeof(std::pair<NodeID, NodeID>) * edges.size());
        out.close();
    }

    std::cout << "Done." << std::endl;
}
