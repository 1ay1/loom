#include "gzip.hpp"
#include <zlib.h>

namespace loom
{
    std::string gzip_compress(const std::string& input)
    {
        z_stream zs{};
        // 15 + 16 = gzip format
        if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
            return {};

        zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
        zs.avail_in = static_cast<uInt>(input.size());

        std::string output;
        output.resize(deflateBound(&zs, input.size()));

        zs.next_out = reinterpret_cast<Bytef*>(output.data());
        zs.avail_out = static_cast<uInt>(output.size());

        deflate(&zs, Z_FINISH);
        output.resize(zs.total_out);
        deflateEnd(&zs);
        return output;
    }
}
