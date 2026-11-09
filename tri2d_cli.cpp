// tri2d_cli.cpp
// Real constrained Delaunay triangulation of a 2D polygon (with optional
// holes) using Geogram's GEO::CDT2d (vendor/geogram-subset), the actual
// equivalent of CGAL's Mesh_2_Constrained_Delaunay_triangulation_2 used by
// GarmentCode's triangulation_utils.py. This replaces the raw-Delaunay +
// domain-filter fallback: GEO::CDT2d supports boundary-edge constraints
// directly (insert_constraint) and automatic inside/outside/hole removal
// (remove_external_triangles(true)), matching CGAL's mark_domains().
//
// Input JSON (stdin):
// {
//   "boundary": [[x,y], ...],       // polygon boundary, closed implicitly
//   "holes": [[[x,y],...], ...],    // optional hole polygons
//   "interior_points": [[x,y],...]  // optional extra interior points
// }
//
// Output JSON (stdout):
// {
//   "points": [[x,y],...],
//   "faces": [[i,j,k],...]          // indices into "points"
// }
#include <geogram/delaunay/CDT_2d.h>
#include <geogram/basic/common.h>
#include <geogram/basic/logger.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <cmath>
#include <mutex>
#include <limits>

using GEO::vec2;
using GEO::index_t;

struct Pt { double x, y; };

// ---- tiny JSON parser (matches this CLI's own fixed schema only) ----
static void skip_ws(const std::string &s, size_t &i) {
    while (i < s.size() && (s[i]==' '||s[i]=='\n'||s[i]=='\t'||s[i]=='\r')) ++i;
}
static double parse_number(const std::string &s, size_t &i) {
    size_t start = i;
    if (s[i]=='-'||s[i]=='+') ++i;
    while (i < s.size() && (isdigit((unsigned char)s[i])||s[i]=='.'||s[i]=='e'||s[i]=='E'||s[i]=='-'||s[i]=='+')) ++i;
    return std::stod(s.substr(start, i-start));
}
static std::vector<Pt> parse_point_array(const std::string &s, size_t &i) {
    std::vector<Pt> out;
    skip_ws(s,i);
    if (i>=s.size() || s[i] != '[') return out;
    ++i; skip_ws(s,i);
    while (i < s.size() && s[i] != ']') {
        skip_ws(s,i);
        if (s[i] == '[') {
            ++i; skip_ws(s,i);
            double x = parse_number(s,i);
            skip_ws(s,i); if (s[i]==',') ++i; skip_ws(s,i);
            double y = parse_number(s,i);
            skip_ws(s,i); if (s[i]==']') ++i;
            out.push_back({x,y});
        }
        skip_ws(s,i);
        if (i<s.size() && s[i]==',') { ++i; skip_ws(s,i); }
    }
    if (i < s.size() && s[i]==']') ++i;
    return out;
}
static bool find_key(const std::string &s, const std::string &key, size_t &i) {
    std::string pat = "\"" + key + "\"";
    size_t p = s.find(pat);
    if (p == std::string::npos) return false;
    p = s.find(':', p);
    if (p == std::string::npos) return false;
    i = p + 1;
    return true;
}
static std::vector<std::vector<Pt>> parse_polygon_array(const std::string &s, size_t &i) {
    std::vector<std::vector<Pt>> out;
    skip_ws(s,i);
    if (i>=s.size() || s[i] != '[') return out;
    ++i; skip_ws(s,i);
    while (i < s.size() && s[i] != ']') {
        if (s[i]=='[') out.push_back(parse_point_array(s,i));
        skip_ws(s,i);
        if (i<s.size() && s[i]==',') { ++i; skip_ws(s,i); }
    }
    if (i<s.size() && s[i]==']') ++i;
    return out;
}

static void ensure_geogram_initialized() {
    static std::once_flag f;
    std::call_once(f, []() {
        GEO::initialize(GEO::GEOGRAM_INSTALL_NONE);
        GEO::Logger::instance()->set_quiet(true);
    });
}

int main() {
    ensure_geogram_initialized();

    std::ostringstream buf;
    buf << std::cin.rdbuf();
    std::string input = buf.str();

    size_t i;
    std::vector<Pt> boundary, interior;
    std::vector<std::vector<Pt>> holes;
    if (find_key(input, "boundary", i)) boundary = parse_point_array(input, i);
    if (find_key(input, "holes", i)) holes = parse_polygon_array(input, i);
    if (find_key(input, "interior_points", i)) interior = parse_point_array(input, i);

    if (boundary.size() < 3) {
        std::cerr << "error: boundary needs >=3 points\n";
        return 1;
    }

    // Flatten all input points: boundary, then each hole in order, then
    // free interior points. Record segment ranges for constraint insertion.
    std::vector<Pt> all_points = boundary;
    std::vector<std::pair<size_t,size_t>> hole_ranges; // [start,end) into all_points
    for (auto &h : holes) {
        size_t start = all_points.size();
        for (auto &p : h) all_points.push_back(p);
        hole_ranges.push_back({start, all_points.size()});
    }
    for (auto &p : interior) all_points.push_back(p);

    // Bounding box with margin for the enclosing rectangle CDT2d requires.
    double minx = std::numeric_limits<double>::infinity(), maxx = -minx;
    double miny = minx, maxy = -minx;
    for (auto &p : all_points) {
        minx = std::min(minx, p.x); maxx = std::max(maxx, p.x);
        miny = std::min(miny, p.y); maxy = std::max(maxy, p.y);
    }
    double dx = std::max(maxx - minx, 1e-9), dy = std::max(maxy - miny, 1e-9);
    double margin = std::max(dx, dy) * 0.2 + 1e-6;

    GEO::CDT2d cdt;
    cdt.create_enclosing_rectangle(minx - margin, miny - margin, maxx + margin, maxy + margin);

    std::vector<double> flat(all_points.size() * 2);
    for (size_t k = 0; k < all_points.size(); ++k) {
        flat[k*2+0] = all_points[k].x;
        flat[k*2+1] = all_points[k].y;
    }
    std::vector<index_t> indices(all_points.size());
    cdt.insert((index_t)all_points.size(), flat.data(), indices.data(), false);

    // Boundary edges: 0-1, 1-2, ..., n-1-0 (in boundary's own index range).
    size_t nb = boundary.size();
    for (size_t k = 0; k < nb; ++k) {
        index_t a = indices[k];
        index_t b = indices[(k + 1) % nb];
        if (a != b) cdt.insert_constraint(a, b);
    }
    // Hole edges.
    for (auto &range : hole_ranges) {
        size_t start = range.first, end = range.second;
        size_t n = end - start;
        if (n < 3) continue;
        for (size_t k = 0; k < n; ++k) {
            index_t a = indices[start + k];
            index_t b = indices[start + (k + 1) % n];
            if (a != b) cdt.insert_constraint(a, b);
        }
    }

    // Remove triangles outside the boundary and inside holes.
    cdt.remove_external_triangles(true);

    index_t nT = cdt.nT();
    index_t nv_total = cdt.nv();

    // Remap to only vertices actually referenced by surviving triangles,
    // in ascending internal-id order, output coords from cdt.point(v).
    std::vector<int> remap(nv_total, -1);
    std::vector<Pt> out_points;
    struct Tri { int a,b,c; };
    std::vector<Tri> out_faces;

    for (index_t t = 0; t < nT; ++t) {
        int vids[3];
        for (int lv = 0; lv < 3; ++lv) {
            index_t v = cdt.Tv(t, lv);
            if (remap[v] == -1) {
                remap[v] = (int)out_points.size();
                vec2 p = cdt.point(v);
                out_points.push_back({p.x, p.y});
            }
            vids[lv] = remap[v];
        }
        out_faces.push_back({vids[0], vids[1], vids[2]});
    }

    std::ostringstream out;
    out.precision(17);
    out << "{\"points\":[";
    for (size_t k = 0; k < out_points.size(); ++k) {
        if (k) out << ",";
        out << "[" << out_points[k].x << "," << out_points[k].y << "]";
    }
    out << "],\"faces\":[";
    for (size_t k = 0; k < out_faces.size(); ++k) {
        if (k) out << ",";
        out << "[" << out_faces[k].a << "," << out_faces[k].b << "," << out_faces[k].c << "]";
    }
    out << "]}";
    std::cout << out.str() << std::endl;
    return 0;
}
