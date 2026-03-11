#include "../../include/loom/http/http.hpp"

#include <sstream>
#include <string>

namespace loom
{
    void Router::add(std::string pattern, RouteHandler handler)
    {
        routes_.push_back({
            std::move(pattern),
            std::move(handler)
        });
    }

    std::string Router::route(const std::string& path)
    {
        for(auto& route: routes_) {
            Params params;
            if(match(route.pattern, path, params))
                return route.handler(params);
        }

        return "<h1>404</h1>";
    }

    bool Router::match(const std::string& pattern,
        const std::string& path,
        Params& params)
    {
        std::stringstream p_stream(pattern);
        std::stringstream u_stream(path);

        std::string pseg;
        std::string useg;

        while (true) {
            bool p_has_segment = static_cast<bool>(std::getline(p_stream, pseg, '/'));
            bool u_has_segment = static_cast<bool>(std::getline(u_stream, useg, '/'));

            // Case 1: Both pattern and path have segments
            if (p_has_segment && u_has_segment) {
                if (pseg.starts_with(':')) {
                    params.push_back(useg); // It's a parameter
                } else if (pseg != useg) {
                    return false; // Segments don't match
                }
                // If segments match or it's a parameter, continue to the next pair
            }
            // Case 2: Pattern has segments, but path is exhausted
            else if (p_has_segment && !u_has_segment) {
                return false; // Pattern expects more segments than path provides
            }
            // Case 3: Path has segments, but pattern is exhausted
            else if (!p_has_segment && u_has_segment) {
                return false; // Path has more segments than pattern allows
            }
            // Case 4: Both pattern and path are exhausted simultaneously
            else if (!p_has_segment && !u_has_segment) {
                return true; // Exact match found
            }
        }
    }
}
