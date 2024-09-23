#include <cassert>
#include <cstdint>

#include "buffered_writer.h"

namespace hyperlink::metis {

struct Header {
    std::uint64_t n = 0;
    std::uint64_t m = 0;
    bool has_vertex_weights = false;
    bool has_edge_weights = false;
};

inline void WriteHeader(BufferedTextOutput<> &out, const Header &header) {
    assert(header.m % 2 == 0 && "number of directed edges must be even");

    out.WriteInt(header.n).WriteChar(' ').WriteInt(header.m / 2);
    if (header.has_vertex_weights || header.has_edge_weights) {
        out.WriteChar(' ');
        if (header.has_vertex_weights) {
            out.WriteInt(header.has_vertex_weights)
                .WriteInt(header.has_edge_weights);
        } else {
            out.WriteInt(header.has_edge_weights);
        }
    }
    out.WriteChar('\n').Flush();
}

template <typename VertexID, typename EdgeID,
          typename VertexWeight = std::int32_t,
          typename EdgeWeight = std::int32_t>
inline void WriteXadjAdjncy(BufferedTextOutput<> &out, const VertexID n,
                            const EdgeID *xadj, const VertexID *adjncy,
                            const VertexWeight *vwgt = nullptr,
                            const EdgeWeight *ewgt = nullptr) {
    const EdgeID offset = xadj[0];

    for (VertexID u = 0; u < n; ++u) {
        if (vwgt != nullptr) {
            out.WriteInt(vwgt[u]).WriteChar(' ').Flush();
        }

        for (EdgeID e = xadj[u]; e < xadj[u + 1]; ++e) {
            out.WriteInt(adjncy[e - offset] + 1).WriteChar(' ');
            if (ewgt != nullptr) {
                out.WriteInt(ewgt[e - offset]).WriteChar(' ');
            }
            out.Flush();
        }

        out.WriteChar('\n').Flush();
    }
}

}  // namespace hyperlink::metis
