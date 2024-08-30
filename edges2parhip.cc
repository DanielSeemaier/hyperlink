#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

using NodeID = std::uint32_t;
using ParhipID = unsigned long long;
using Edge = std::pair<NodeID, NodeID>;

ParhipID BuildVersion(const bool has_vertex_weights,
                      const bool has_edge_weights,
                      const bool has_32bit_edge_ids,
                      const bool has_32bit_vertex_ids,
                      const bool has_32bit_vertex_weights,
                      const bool has_32bit_edge_weights) {
    const ParhipID edge_weights_bit =
        static_cast<ParhipID>(has_edge_weights) ^ 1;
    const ParhipID vertex_weights_bit =
        (static_cast<ParhipID>(has_vertex_weights) ^ 1) << 1;
    const ParhipID edge_id_width_bit = static_cast<ParhipID>(has_32bit_edge_ids)
                                       << 2;
    const ParhipID vertex_id_width_bit =
        static_cast<ParhipID>(has_32bit_vertex_ids) << 3;
    const ParhipID vertex_weight_width_bit =
        static_cast<ParhipID>(has_32bit_vertex_weights) << 3;
    const ParhipID edge_weight_width_bit =
        static_cast<ParhipID>(has_32bit_edge_weights) << 4;

    return vertex_weights_bit | edge_weights_bit | edge_id_width_bit |
           vertex_id_width_bit | vertex_weight_width_bit |
           edge_weight_width_bit;
}

std::size_t file_size(std::ifstream &in) {
    in.seekg(0, std::ios::end);
    const std::size_t size = in.tellg();
    in.seekg(0, std::ios::beg);
    return size;
}

template <std::size_t buf_size = 1ull * 1024 * 1024>
class Merger {
   public:
    Merger(std::ifstream &in_a, std::ifstream &in_b)
        : _ins{&in_a, &in_b},
          _sizes{file_size(in_a), file_size(in_b)},
          _curs{0, 0},
          _bufs{std::vector<Edge>{}, std::vector<Edge>{}} {
        for (std::size_t B = 0; B < _bufs.size(); ++B) {
            _bufs[B].resize(buf_size);
            refill(B);
        }
    }

    template <typename Lambda>
    void for_each_edge(Lambda &&l) {
        Edge a, b;
        bool has_a, has_b;

        while ((has_a = get(0, a)) | (has_b = get(1, b))) {
            if (!has_b || (has_a && a < b)) {
                l(a);
                advance(0);
            } else {
                l(b);
                advance(1);
            }
        }
    }

   private:
    bool refill(const std::size_t B) {
        if (_curs[B] == _sizes[B]) {
            return false;
        }

        const std::size_t N =
            std::min(buf_size * sizeof(Edge), _sizes[B] - _curs[B]);
        _ins[B]->read(reinterpret_cast<char *>(_bufs[B].data()), N);
        return true;
    }

    bool get(const std::size_t B, Edge &edge) {
        if (_curs[B] == _sizes[B]) {
            return false;
        }

        edge = _bufs[B][(_curs[B] / sizeof(Edge)) % buf_size];
        return true;
    }

    void advance(const std::size_t B) {
        _curs[B] += sizeof(Edge);
        if ((_curs[B] % (buf_size * sizeof(Edge))) == 0) {
            refill(B);
        }
    }

    std::array<std::ifstream *, 2> _ins;
    std::array<std::size_t, 2> _sizes;
    std::array<std::size_t, 2> _curs;
    std::array<std::vector<Edge>, 2> _bufs;
};

int main(const int argc, const char *argv[]) {
    if (argc != 4) {
        std::cerr << "usage: ./edges2parhip <input.bin> <input.rev.bin> "
                     "<output.parhip>\n";
        std::exit(1);
    }

    const std::string input_a_filename = argv[1];
    const std::string input_b_filename = argv[2];
    const std::string output_filename = argv[3];

    if (std::ifstream test_out(output_filename, std::ios::binary); test_out) {
        std::cerr << "error: output file already exists\n";
        std::exit(1);
    }

    std::ifstream in_a(input_a_filename, std::ios::binary);
    if (!in_a) {
        std::cerr << "error: could not open first input file\n";
        std::exit(1);
    }

    std::ifstream in_b(input_b_filename, std::ios::binary);
    if (!in_b) {
        std::cerr << "error: could not open second input file\n";
        std::exit(1);
    }

    if (file_size(in_a) != file_size(in_b)) {
        std::cerr << "error: input files have different sizes\n";
        std::exit(1);
    }

    std::ofstream out(output_filename, std::ios::binary | std::ios_base::trunc);

    std::vector<ParhipID> xadj;

    std::cout << "Counting degrees ..." << std::endl;

    // First pass for xadj
    {
        Merger<> merger(in_a, in_b);
        merger.for_each_edge([&](const Edge &edge) {
            const auto &[u, v] = edge;
            while (xadj.size() <= u) {
                xadj.push_back(0);
                if (xadj.size() % (1ull * 1024 * 1024) == 0) {
                    std::cout << "\t" << xadj.size() << " nodes, " << u
                              << " ..." << std::endl;
                }
            }
            ++xadj[u];
        });
    }

    std::cout << "Computing prefix sum for xadj[] ..." << std::endl;

    const ParhipID n = xadj.size();
    xadj.push_back(0);
    std::exclusive_scan(xadj.begin(), xadj.end(), xadj.begin(), static_cast<ParhipID>(0));
    const ParhipID m = xadj.back();

    std::cout << "There are " << n << " nodes and " << m << " edges"
              << std::endl;
    std::cout << "Adding offsets to xadj[] ..." << std::endl;

    for (ParhipID &x : xadj) {
        x = 3 * sizeof(ParhipID) + (n + 1) * sizeof(ParhipID) +
            x * sizeof(NodeID);
    }

    std::cout << "Writing xadj[] to output file ..." << std::endl;

    const ParhipID version =
        BuildVersion(false, false, false, true, false, false);
    out.write(reinterpret_cast<const char *>(&version), sizeof(ParhipID));
    out.write(reinterpret_cast<const char *>(&n), sizeof(ParhipID));
    out.write(reinterpret_cast<const char *>(&m), sizeof(ParhipID));
    out.write(reinterpret_cast<const char *>(xadj.data()),
              xadj.size() * sizeof(ParhipID));

    constexpr std::size_t buf_size = 1ull * 1 * 1024 * 1024;
    std::vector<NodeID> adjncy;
    adjncy.reserve(buf_size);

    std::cout << "Reading and writing adjncy[] ..." << std::endl;

    // Second pass for adjncy
    {
        Merger<> merger(in_a, in_b);
        merger.for_each_edge([&](const Edge &edge) {
            adjncy.push_back(edge.second);
            if (adjncy.size() == buf_size) {
                out.write(reinterpret_cast<const char *>(adjncy.data()),
                          adjncy.size() * sizeof(NodeID));
                adjncy.clear();
            }
        });
        if (!adjncy.empty()) {
            out.write(reinterpret_cast<const char *>(adjncy.data()),
                      adjncy.size() * sizeof(NodeID));
        }
    }

    std::cout << "Done." << std::endl;
}
