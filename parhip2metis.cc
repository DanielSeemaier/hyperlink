#include <fstream>
#include <iostream>

#include "buffered_writer.h"
#include "metis.h"
#include "parhip.h"

using namespace hyperlink;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "usage: ./parhip2metis <input.parhip> <output.metis> "
                     "[<chunk size>]\n";
        std::exit(1);
    }

    const std::string input_filename = argv[1];
    const std::string output_filename = argv[2];
    const std::uint64_t chunk_size =
        argc == 4 ? std::stoull(argv[3])
                  : std::numeric_limits<std::uint64_t>::max();

    std::cout << "In(parhip): " << input_filename << std::endl;
    std::cout << "Out(metis): " << output_filename << std::endl;
    if (chunk_size != std::numeric_limits<std::uint64_t>::max()) {
        std::cout << "Chunk size: " << chunk_size << std::endl;
    }

    BufferedTextOutput<> out(tag::create, output_filename);

    std::ifstream in(input_filename, std::ios::binary);
    if (!in) {
        std::cerr << "error: could not open input file\n";
        std::exit(1);
    }

    std::cout << "Reading header ..." << std::endl;
    const parhip::Header parhip_header = parhip::ReadHeader(in);

    std::cout << "\tNumber of vertices: " << parhip_header.n << std::endl;
    std::cout << "\tNumber of edges: " << parhip_header.m << std::endl;
    std::cout << "\tVertex weights: "
              << (parhip_header.version.has_vertex_weights ? "yes" : "no")
              << std::endl;
    std::cout << "\tEdge weights: "
              << (parhip_header.version.has_edge_weights ? "yes" : "no")
              << std::endl;
    std::cout << "\tVertex ID width: "
              << (parhip_header.version.has_32bit_vertex_ids ? "32" : "64")
              << " bits" << std::endl;
    std::cout << "\tEdge ID width: "
              << (parhip_header.version.has_32bit_edge_ids ? "32" : "64")
              << " bits" << std::endl;
    std::cout << "\tVertex weight width: "
              << (parhip_header.version.has_32bit_vertex_weights ? "32" : "64")
              << " bits" << std::endl;
    std::cout << "\tEdge weight width: "
              << (parhip_header.version.has_32bit_edge_weights ? "32" : "64")
              << " bits" << std::endl;

    metis::Header metis_header{
        .n = parhip_header.n,
        .m = parhip_header.m,
        .has_vertex_weights = parhip_header.version.has_vertex_weights,
        .has_edge_weights = parhip_header.version.has_edge_weights,
    };
    metis::WriteHeader(out, metis_header);

    std::cout << "Reading xadj[] array ..." << std::endl;
    const auto xadj_data = parhip::ReadXadj(in, parhip_header);
    std::cout << "\tSize: " << xadj_data.size() << " bytes" << std::endl;

    parhip::Data adjncy_data;

    std::cout << "Copying adjacency lists " << std::flush;
    for (std::uint64_t u = 0; u < parhip_header.n; u += chunk_size) {
        const std::uint64_t n = parhip::ReadAdjncy(
            in, adjncy_data, parhip_header, xadj_data, u, u + chunk_size);

        std::cout << "." << std::flush;

        parhip::DecodeXadjAdjncy(
            parhip_header, xadj_data, adjncy_data,
            [&]<typename EdgeID, typename VertexID>(const EdgeID *xadj,
                                                    const VertexID *adjncy) {
                metis::WriteXadjAdjncy<VertexID, EdgeID>(out, n, xadj + u,
                                                         adjncy);
            });
    }
    std::cout << std::endl;

    std::cout << "Done." << std::endl;
}
