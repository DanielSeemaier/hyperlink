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
    if (argc != 4) {
        std::cerr << "usage: ./txt2sbin <input.txt> <output.bin> <upper bound "
                     "on the number of edges in billions>\n";
        std::exit(1);
    }

    const std::string input_filename = argv[1];
    const std::string output_filename = argv[2];
    const std::uint64_t max_edges =
        static_cast<std::uint64_t>(std::stoull(argv[3]) * 1'000'000'000);

    if (std::ifstream in(input_filename); !in) {
        std::cerr << "error: could not open input file\n";
        std::exit(1);
    }

    if (std::ofstream out(output_filename, std::ios::binary); !out) {
        std::cerr << "error: could not open output file\n";
        std::exit(1);
    }

    std::vector<std::pair<NodeID, NodeID>> edges;
    edges.reserve(max_edges);

    std::cout << "Preallocated edge buffer: "
              << (sizeof(std::pair<NodeID, NodeID>) * max_edges) / 1024 / 1024 /
                     1024
              << " GB" << std::endl;

    MappedFileToker toker(input_filename);

    std::cout << "Parsing input file ... ("
              << toker.length() / 1024 / 1024 / 1024 << " GB) ..." << std::endl;

    toker.skip_spaces();
    std::uint64_t self_loops_removed = 0;
    while (toker.valid_position()) {
        const NodeID u = static_cast<NodeID>(toker.scan_uint());
        const NodeID v = static_cast<NodeID>(toker.scan_uint());

        if (u < v) {
            edges.emplace_back(u, v);
        } else if (v < u) {
            edges.emplace_back(v, u);
        } else {
            ++self_loops_removed;
        }

        if (edges.size() % 100'000'000 == 0) {
            std::cout << "\t" << toker.position() / 1024 / 1024 / 1024
                      << " GB, removed " << self_loops_removed
                      << " self-loops (= "
                      << sizeof(std::pair<NodeID, NodeID>) *
                             self_loops_removed / 1024 / 1024 / 1024
                      << " GB)..." << std::endl;
        }
    }

    std::cout << "Sorting edges ..." << std::endl;
    ips4o::parallel::sort(edges.begin(), edges.end());

    const std::uint64_t edges_before_removing_duplicates = edges.size();
    std::cout << "Removing duplicate edges ..." << std::endl;
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    const std::uint64_t edges_after_removing_duplicates = edges.size();
    const std::uint64_t duplicates_removed =
        edges_before_removing_duplicates - edges_after_removing_duplicates;
    std::cout << "\tRemoved " << duplicates_removed << " duplicates (= "
              << sizeof(std::pair<NodeID, NodeID>) * duplicates_removed / 1024 /
                     1024 / 1024
              << " GB)" << std::endl;

    std::cout << "Writing output file ..." << std::endl;

    std::ofstream output(output_filename, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char *>(edges.data()),
                 sizeof(std::pair<NodeID, NodeID>) * edges.size());

    std::cout << "Done." << std::endl;
    std::cout << "\tEdges read:         "
              << edges.size() + duplicates_removed + self_loops_removed
              << std::endl;
    std::cout << "\tEdges kept:         " << edges.size() << std::endl;
    std::cout << "\tDuplicates removed: " << duplicates_removed << std::endl;
    std::cout << "\tSelf-loops removed: " << self_loops_removed << std::endl;
}
