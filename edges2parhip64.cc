#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

using NodeID = std::uint64_t;
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
    Merger(std::vector<std::ifstream> &ins) : _ins(ins), _B(_ins.size()) {
        for (auto &in : _ins) {
            _sizes.push_back(file_size(in));
            _curs.push_back(0);

            _bufs.emplace_back(buf_size);
            refill(_bufs.size() - 1);
        }
    }

    template <typename Lambda>
    void for_each_edge(Lambda &&l) {
        // B = 4
        // [0, 1, 2, 3, 4, 5, 6, 7]
        //              ^  ^  ^  ^
        //           ^--------^--^
        //        ^-----^--^
        //     ^--^--^
        std::vector<std::pair<NodeID, int>> tree(2 * _B);
        for (std::size_t b = 0; b < _B; ++b) {
            tree[_B + b] = {get(b).first, b};
        }
        for (std::size_t i = _B - 1; i > 0; --i) {
            tree[i] = std::min(tree[i * 2], tree[i * 2 + 1]);
        }

        while (tree[1].first < std::numeric_limits<NodeID>::max()) {
            const auto &[_, b] = tree[1];

            l(get(b));
            advance(b);

            auto winner = tree[_B + b] = {get(b).first, b};
            for (std::size_t i = (_B + b) >> 1; i > 0; i >>= 1) {
                tree[i] = std::min(tree[i * 2], tree[i * 2 + 1]);
            }
        }
    }

   private:
    bool refill(const std::size_t b) {
        if (_curs[b] == _sizes[b]) {
            return false;
        }

        const std::size_t N =
            std::min(buf_size * sizeof(Edge), _sizes[b] - _curs[b]);
        _ins[b].read(reinterpret_cast<char *>(_bufs[b].data()), N);
        return true;
    }

    Edge get(const std::size_t b) {
        if (_curs[b] == _sizes[b]) {
            return {std::numeric_limits<NodeID>::max(),
                    std::numeric_limits<NodeID>::max()};
        }

        return _bufs[b][(_curs[b] / sizeof(Edge)) % buf_size];
    }

    void advance(const std::size_t b) {
        _curs[b] += sizeof(Edge);
        if ((_curs[b] % (buf_size * sizeof(Edge))) == 0) {
            refill(b);
        }
    }

    std::vector<std::ifstream> &_ins;
    std::size_t _B;

    std::vector<std::size_t> _sizes = {};
    std::vector<std::size_t> _curs = {};
    std::vector<std::vector<Edge>> _bufs = {};
};

int main(const int argc, const char *argv[]) {
    if (argc < 3) {
        std::cerr << "usage: ./edges2parhip <output.parhip> <inputs...>\n";
        std::exit(1);
    }

    const std::string output_filename = argv[1];

    std::vector<std::ifstream> ins;
    for (std::size_t b = 2; b < argc; ++b) {
        ins.emplace_back(argv[b], std::ios::binary);
        if (!ins.back()) {
            std::cerr << "error: cannot read input buffer " << argv[b] << "\n";
            std::exit(1);
        }
    }

    std::ofstream out(output_filename, std::ios::binary | std::ios_base::trunc);
    if (!out) {
        std::cerr << "error: cannot write to output buffer " << output_filename
                  << "\n";
        std::exit(1);
    }

    std::vector<ParhipID> xadj;

    std::cout << "Counting degrees ..." << std::endl;

    // First pass for xadj
    {
        Merger<> merger(ins);
        merger.for_each_edge([&](const Edge &edge) {
            const auto &[u, v] = edge;
            while (xadj.size() <= u) {
                xadj.push_back(0);
                if (xadj.size() % (1ull * 1024 * 1024 * 1024) == 0) {
                    std::cout << "\t" << xadj.size() << " nodes ..."
                              << std::endl;
                }
            }
            ++xadj[u];
        });
    }

    std::cout << "Computing prefix sum for xadj[] ..." << std::endl;

    const ParhipID n = xadj.size();
    xadj.push_back(0);
    std::exclusive_scan(xadj.begin(), xadj.end(), xadj.begin(),
                        static_cast<ParhipID>(0));
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
        BuildVersion(false, false, false, sizeof(NodeID) == 4, false, false);
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
        Merger<> merger(ins);
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
