#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

namespace hyperlink::parhip {

using ID64 = unsigned long long;
using ID32 = std::uint32_t;

struct Version {
    bool has_edge_weights;
    bool has_vertex_weights;
    bool has_32bit_edge_ids;
    bool has_32bit_vertex_ids;
    bool has_32bit_vertex_weights;
    bool has_32bit_edge_weights;
};

struct Header {
    Version version;
    ID64 n;
    ID64 m;
};

using Data = std::vector<char>;

inline ID64 EncodeVersion(const Version &version) {
    // To be compatible with the original format used by ParHiP, we use the
    // following versions:
    //
    // 3 = no vertex or edge weights = compatible with ParHiP
    // 2 = no vertex weights, but edge weights
    // 1 = vertex weights, but no edge weights
    // 0 = vertex weights and edge weights
    //
    // I.e., the negated "format" code used by the Metis format Higher bits are
    // used to encode the data types used to store the graph; if set to 0, we
    // use 64 bits

    const ID64 edge_weights_bit =
        static_cast<int>(version.has_edge_weights) ^ 1;
    const ID64 vertex_weights_bit =
        (static_cast<int>(version.has_vertex_weights) ^ 1) << 1;
    const ID64 edge_id_width_bit = static_cast<int>(version.has_32bit_edge_ids)
                                   << 2;
    const ID64 vertex_id_width_bit =
        static_cast<int>(version.has_32bit_vertex_ids) << 3;
    const ID64 vertex_weight_width_bit =
        static_cast<int>(version.has_32bit_vertex_weights) << 3;
    const ID64 edge_weight_width_bit =
        static_cast<int>(version.has_32bit_edge_weights) << 4;

    return vertex_weights_bit | edge_weights_bit | edge_id_width_bit |
           vertex_id_width_bit | vertex_weight_width_bit |
           edge_weight_width_bit;
}

inline Version DecodeVersion(const ID64 version) {
    return {
        .has_edge_weights = (version & 1) == 0,
        .has_vertex_weights = (version & 2) == 0,
        .has_32bit_edge_ids = (version & 4) != 0,
        .has_32bit_vertex_ids = (version & 8) != 0,
        .has_32bit_vertex_weights = (version & 16) != 0,
        .has_32bit_edge_weights = (version & 32) != 0,
    };
}

inline Header ReadHeader(std::ifstream &in) {
    ID64 version, n, m;
    in.read(reinterpret_cast<char *>(&version), sizeof(ID64));
    in.read(reinterpret_cast<char *>(&n), sizeof(ID64));
    in.read(reinterpret_cast<char *>(&m), sizeof(ID64));
    assert(!in.rdstate() && "failed to read header");

    return {
        .version = DecodeVersion(version),
        .n = n,
        .m = m,
    };
}

inline int VertexIDWidth(const Header &header) {
    return header.version.has_32bit_vertex_ids ? 4 : 8;
}

inline int VertexIDShift(const Header &header) {
    return header.version.has_32bit_vertex_ids ? 2 : 3;
}

inline int EdgeIDWidth(const Header &header) {
    return header.version.has_32bit_edge_ids ? 4 : 8;
}

inline int EdgeIDShift(const Header &header) {
    return header.version.has_32bit_edge_ids ? 2 : 3;
}

inline int VertexWeightWidth(const Header &header) {
    return header.version.has_32bit_vertex_weights ? 4 : 8;
}

inline int VertexWeightShift(const Header &header) {
    return header.version.has_32bit_vertex_weights ? 2 : 3;
}

inline int EdgeWeightWidth(const Header &header) {
    return header.version.has_32bit_edge_weights ? 4 : 8;
}

inline int EdgeWeightShift(const Header &header) {
    return header.version.has_32bit_edge_weights ? 2 : 3;
}

template <typename Lambda>
inline void DecodeXadj(const Header &header, const Data &xadj, Lambda &&l) {
    if (header.version.has_32bit_edge_ids) {
        l(reinterpret_cast<const ID32 *>(xadj.data()));
    } else {
        l(reinterpret_cast<const ID64 *>(xadj.data()));
    }
}

template <typename Lambda>
inline void DecodeAdjncy(const Header &header, const Data &adjncy, Lambda &&l) {
    if (header.version.has_32bit_vertex_ids) {
        l(reinterpret_cast<const ID32 *>(adjncy.data()));
    } else {
        l(reinterpret_cast<const ID64 *>(adjncy.data()));
    }
}

template <typename Lambda>
inline void DecodeXadjAdjncy(const Header &header, const Data &xadj_data,
                             const Data &adjncy_data, Lambda &&l) {
    DecodeXadj(header, xadj_data, [&](const auto *xadj) {
        DecodeAdjncy(header, adjncy_data,
                     [&](const auto *adjncy) { l(xadj, adjncy); });
    });
}

inline Data ReadXadj(std::ifstream &in, const Header &header) {
    const std::size_t xadj_offset = 3 * sizeof(ID64);
    in.seekg(xadj_offset, std::ios_base::beg);
    assert(!in.rdstate() && "failed to seek to xadj");

    const std::size_t nbytes = (header.n + 1) * EdgeIDWidth(header);
    Data xadj(nbytes);
    in.read(xadj.data(), nbytes);
    assert(!in.rdstate() && "failed to read xadj");

    const int shift = VertexIDShift(header);
    auto normalize_xadj = [&]<typename ID>(ID *xadj) {
        const ID offset = xadj[0];

        for (std::size_t i = 0; i < header.n + 1; ++i) {
            xadj[i] = (xadj[i] - offset) >> shift;
        }
    };

    if (header.version.has_32bit_edge_ids) {
        normalize_xadj(reinterpret_cast<ID32 *>(xadj.data()));
    } else {  // 64-bit edge ids
        normalize_xadj(reinterpret_cast<ID64 *>(xadj.data()));
    }

    return xadj;
}

inline ID64 ReadAdjncy(std::ifstream &from, Data &to, const Header &header,
                       const Data &xadj, const ID64 begin_vertex = 0,
                       ID64 end_vertex = std::numeric_limits<ID64>::max()) {
    end_vertex = std::min<ID64>(end_vertex, header.n);
    assert(end_vertex < xadj.size() && "invalid xadj[] size");

    auto read_xadj = [&](const std::size_t i) -> ID64 {
        if (header.version.has_32bit_edge_ids) {
            return reinterpret_cast<const ID32 *>(xadj.data())[i];
        } else {
            return reinterpret_cast<const ID64 *>(xadj.data())[i];
        }
    };

    ID64 adjncy_offset = 3 * sizeof(ID64);
    adjncy_offset += (header.n + 1) * EdgeIDWidth(header);
    adjncy_offset += read_xadj(begin_vertex) * VertexIDWidth(header);
    from.seekg(adjncy_offset, std::ios_base::beg);

    const std::size_t nbytes =
        (read_xadj(end_vertex) - read_xadj(begin_vertex)) *
        VertexIDWidth(header);

    to.resize(nbytes);

    from.read(to.data(), nbytes);
    assert(!from.rdstate() && "failed to read adjncy");

    return end_vertex - begin_vertex;
}

}  // namespace hyperlink::parhip
