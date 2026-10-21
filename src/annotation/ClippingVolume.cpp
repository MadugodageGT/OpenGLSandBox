#include "ClippingVolume.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_map>

namespace clipvol {

namespace {

const float kBig = (std::numeric_limits<float>::max)();

float Cross2(const glm::vec2& a, const glm::vec2& b) { return a.x * b.y - a.y * b.x; }

float SignedArea2(const std::vector<glm::vec2>& p)
{
    float s = 0.0f;
    for (size_t i = 0; i < p.size(); ++i)
        s += Cross2(p[i], p[(i + 1) % p.size()]);
    return s;
}

// ---------------------------------------------------------------------------------------------
// Polygon helpers
// ---------------------------------------------------------------------------------------------

bool OnSegment(const glm::vec2& a, const glm::vec2& b, const glm::vec2& p)
{
    return (std::min)(a.x, b.x) <= p.x && p.x <= (std::max)(a.x, b.x) &&
           (std::min)(a.y, b.y) <= p.y && p.y <= (std::max)(a.y, b.y);
}

// True if the closed segments ab and cd share at least one point.
bool SegmentsIntersect(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, const glm::vec2& d)
{
    const float eps = 1e-3f;
    const float d1 = Cross2(b - a, c - a);
    const float d2 = Cross2(b - a, d - a);
    const float d3 = Cross2(d - c, a - c);
    const float d4 = Cross2(d - c, b - c);

    if (((d1 > eps && d2 < -eps) || (d1 < -eps && d2 > eps)) &&
        ((d3 > eps && d4 < -eps) || (d3 < -eps && d4 > eps)))
        return true;

    if (std::fabs(d1) <= eps && OnSegment(a, b, c)) return true;
    if (std::fabs(d2) <= eps && OnSegment(a, b, d)) return true;
    if (std::fabs(d3) <= eps && OnSegment(c, d, a)) return true;
    if (std::fabs(d4) <= eps && OnSegment(c, d, b)) return true;
    return false;
}

bool SelfIntersects(const std::vector<glm::vec2>& p)
{
    const size_t n = p.size();
    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = i + 1; j < n; ++j)
        {
            const bool adjacent = (j == i + 1) || (i == 0 && j == n - 1);
            if (adjacent)
                continue;
            if (SegmentsIntersect(p[i], p[(i + 1) % n], p[j], p[(j + 1) % n]))
                return true;
        }
    }
    return false;
}

// Inclusive point-in-triangle test for a CCW triangle.
bool PointInTriangle(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c)
{
    const float eps = 1e-4f;
    return Cross2(b - a, p - a) >= -eps &&
           Cross2(c - b, p - b) >= -eps &&
           Cross2(a - c, p - c) >= -eps;
}

// ---------------------------------------------------------------------------------------------
// Triangle rasterisation on the CPU (pixel centres at +0.5), used for the depth look-ups
// ---------------------------------------------------------------------------------------------

template <typename Fn>
void ForEachPixelInTriangle(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c,
                            int W, int H, Fn fn)
{
    const float area2 = Cross2(b - a, c - a);
    if (std::fabs(area2) < 1e-6f)
        return;

    const float minX = (std::min)({ a.x, b.x, c.x });
    const float maxX = (std::max)({ a.x, b.x, c.x });
    const float minY = (std::min)({ a.y, b.y, c.y });
    const float maxY = (std::max)({ a.y, b.y, c.y });

    const int x0 = (std::max)(0, static_cast<int>(std::floor(minX - 0.5f)));
    const int x1 = (std::min)(W - 1, static_cast<int>(std::ceil(maxX - 0.5f)));
    const int y0 = (std::max)(0, static_cast<int>(std::floor(minY - 0.5f)));
    const int y1 = (std::min)(H - 1, static_cast<int>(std::ceil(maxY - 0.5f)));

    const float inv = 1.0f / area2;
    const float eps = 1e-4f;

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            const glm::vec2 p(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
            const float la = Cross2(b - p, c - p) * inv;
            const float lb = Cross2(c - p, a - p) * inv;
            const float lc = Cross2(a - p, b - p) * inv;
            if (la >= -eps && lb >= -eps && lc >= -eps)
                fn(x, y, la, lb, lc);
        }
    }
}

// 3x3 min / max filters over the surface pixels of a depth map. Using these (instead of the raw
// samples) makes the prism conservative against sub-pixel surface variation between pixel centres.
void BuildMinMax3x3(const DepthMap& d, std::vector<float>& dmin, std::vector<float>& dmax)
{
    const int W = d.width;
    const int H = d.height;
    dmin.assign(static_cast<size_t>(W) * H, kBig);
    dmax.assign(static_cast<size_t>(W) * H, -kBig);

    for (int y = 0; y < H; ++y)
    {
        for (int x = 0; x < W; ++x)
        {
            float lo = kBig;
            float hi = -kBig;
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const int xx = x + dx;
                    const int yy = y + dy;
                    if (xx < 0 || yy < 0 || xx >= W || yy >= H)
                        continue;
                    const float v = d.At(xx, yy);
                    if (!IsSurface(v))
                        continue;
                    lo = (std::min)(lo, v);
                    hi = (std::max)(hi, v);
                }
            }
            dmin[static_cast<size_t>(y) * W + x] = lo;
            dmax[static_cast<size_t>(y) * W + x] = hi;
        }
    }
}

// ---------------------------------------------------------------------------------------------
// 2D triangulation with conforming longest-edge refinement (Rivara)
// ---------------------------------------------------------------------------------------------

struct Mesh2D
{
    std::vector<glm::vec2> P;
    std::vector<std::array<uint32_t, 3>> T;
    std::vector<char> alive;

    int AliveCount() const
    {
        int n = 0;
        for (size_t i = 0; i < alive.size(); ++i)
            n += alive[i] ? 1 : 0;
        return n;
    }

    float TriAreaPx(int t) const
    {
        const glm::vec2& a = P[T[t][0]];
        const glm::vec2& b = P[T[t][1]];
        const glm::vec2& c = P[T[t][2]];
        return 0.5f * std::fabs(Cross2(b - a, c - a));
    }

    // Total order on edges (length, then indices) so that two triangles sharing an edge always agree on
    // which of their edges is the "longest".
    bool Longer(uint32_t a0, uint32_t b0, uint32_t a1, uint32_t b1) const
    {
        const glm::vec2 d0 = P[b0] - P[a0];
        const glm::vec2 d1 = P[b1] - P[a1];
        const float l0 = glm::dot(d0, d0);
        const float l1 = glm::dot(d1, d1);
        if (l0 != l1)
            return l0 > l1;
        const uint32_t lo0 = (std::min)(a0, b0), hi0 = (std::max)(a0, b0);
        const uint32_t lo1 = (std::min)(a1, b1), hi1 = (std::max)(a1, b1);
        if (lo0 != lo1)
            return lo0 < lo1;
        return hi0 < hi1;
    }

    // Edge k of triangle t runs from T[t][k] to T[t][(k+1)%3].
    int LongestEdge(int t) const
    {
        int best = 0;
        for (int k = 1; k < 3; ++k)
        {
            if (Longer(T[t][k], T[t][(k + 1) % 3], T[t][best], T[t][(best + 1) % 3]))
                best = k;
        }
        return best;
    }

    int FindTriangleWithDirectedEdge(uint32_t u, uint32_t v) const
    {
        for (size_t t = 0; t < T.size(); ++t)
        {
            if (!alive[t])
                continue;
            for (int k = 0; k < 3; ++k)
            {
                if (T[t][k] == u && T[t][(k + 1) % 3] == v)
                    return static_cast<int>(t);
            }
        }
        return -1;
    }

    void AddTri(uint32_t a, uint32_t b, uint32_t c)
    {
        std::array<uint32_t, 3> tri = { a, b, c };
        T.push_back(tri);
        alive.push_back(1);
    }

    // Splits the longest edge of triangle t (and, to stay conforming, the triangle across it).
    // Neighbours whose longest edge is a different one are refined first (classic Rivara propagation).
    void Refine(int t)
    {
        while (alive[t])
        {
            const int k = LongestEdge(t);
            const uint32_t u = T[t][k];
            const uint32_t v = T[t][(k + 1) % 3];
            const uint32_t w = T[t][(k + 2) % 3];

            const int nb = FindTriangleWithDirectedEdge(v, u);
            if (nb >= 0)
            {
                const int kn = LongestEdge(nb);
                const bool sameEdge = (T[nb][kn] == v && T[nb][(kn + 1) % 3] == u);
                if (!sameEdge)
                {
                    Refine(nb);
                    continue;
                }
            }

            const uint32_t m = static_cast<uint32_t>(P.size());
            P.push_back(0.5f * (P[u] + P[v]));

            alive[t] = 0;
            AddTri(u, m, w);
            AddTri(m, v, w);

            if (nb >= 0)
            {
                uint32_t x = 0;
                for (int j = 0; j < 3; ++j)
                {
                    if (T[nb][j] == v && T[nb][(j + 1) % 3] == u)
                        x = T[nb][(j + 2) % 3];
                }
                alive[nb] = 0;
                AddTri(v, m, x);
                AddTri(m, u, x);
            }
            return;
        }
    }
};

// ---------------------------------------------------------------------------------------------
// Offsets (paper Fig. 8b): minimal front/back depth that encloses the surface under every vertex
// ---------------------------------------------------------------------------------------------

bool ComputeOffsets(const Mesh2D& m, const DepthMap& D1,
                    const std::vector<float>& dmin, const std::vector<float>& dmax,
                    float margin, std::vector<float>& front, std::vector<float>& back)
{
    const int W = D1.width;
    const int H = D1.height;
    const size_t nv = m.P.size();

    std::vector<float> lo(nv, kBig);
    std::vector<float> hi(nv, -kBig);

    for (size_t t = 0; t < m.T.size(); ++t)
    {
        if (!m.alive[t])
            continue;

        const glm::vec2& a = m.P[m.T[t][0]];
        const glm::vec2& b = m.P[m.T[t][1]];
        const glm::vec2& c = m.P[m.T[t][2]];

        float tmin = kBig;
        float tmax = -kBig;

        auto sample = [&](int x, int y)
        {
            if (x < 0 || y < 0 || x >= W || y >= H)
                return;
            const size_t i = static_cast<size_t>(y) * W + x;
            if (dmin[i] <= dmax[i])
            {
                tmin = (std::min)(tmin, dmin[i]);
                tmax = (std::max)(tmax, dmax[i]);
            }
        };

        ForEachPixelInTriangle(a, b, c, W, H, [&](int x, int y, float, float, float) { sample(x, y); });

        // Tiny triangles may contain no pixel centre: always sample the corners and the centroid.
        const glm::vec2 centroid = (a + b + c) / 3.0f;
        sample(static_cast<int>(std::floor(a.x)), static_cast<int>(std::floor(a.y)));
        sample(static_cast<int>(std::floor(b.x)), static_cast<int>(std::floor(b.y)));
        sample(static_cast<int>(std::floor(c.x)), static_cast<int>(std::floor(c.y)));
        sample(static_cast<int>(std::floor(centroid.x)), static_cast<int>(std::floor(centroid.y)));

        if (tmin > tmax)
            continue;

        // A vertex shared by several triangles takes the union of their ranges, so the linearly
        // interpolated front/back faces enclose every incident triangle's surface chunk.
        for (int k = 0; k < 3; ++k)
        {
            lo[m.T[t][k]] = (std::min)(lo[m.T[t][k]], tmin);
            hi[m.T[t][k]] = (std::max)(hi[m.T[t][k]], tmax);
        }
    }

    // Vertices that saw no surface at all (outline partly over the background): borrow from the
    // closest vertex that did.
    bool any = false;
    for (size_t i = 0; i < nv; ++i)
        any = any || (lo[i] <= hi[i]);
    if (!any)
        return false;

    for (size_t i = 0; i < nv; ++i)
    {
        if (lo[i] <= hi[i])
            continue;
        float bestD = kBig;
        size_t bestJ = i;
        for (size_t j = 0; j < nv; ++j)
        {
            if (lo[j] > hi[j])
                continue;
            const float d = glm::distance(m.P[i], m.P[j]);
            if (d < bestD)
            {
                bestD = d;
                bestJ = j;
            }
        }
        lo[i] = lo[bestJ];
        hi[i] = hi[bestJ];
    }

    front.assign(nv, 0.0f);
    back.assign(nv, 1.0f);
    for (size_t i = 0; i < nv; ++i)
    {
        front[i] = (std::max)(0.0f, lo[i] - margin);
        back[i] = (std::min)(1.0f, hi[i] + margin);
    }
    return true;
}

// Triangles whose prism still contains a second surface layer (paper Fig. 8d).
std::vector<int> FindContaminated(const Mesh2D& m, const DepthMap& D1, const DepthMap& D2,
                                  const std::vector<float>& back, float minAreaPx)
{
    std::vector<int> flagged;
    const int W = D1.width;
    const int H = D1.height;

    for (size_t t = 0; t < m.T.size(); ++t)
    {
        if (!m.alive[t])
            continue;
        if (m.TriAreaPx(static_cast<int>(t)) < minAreaPx)
            continue;

        const glm::vec2& a = m.P[m.T[t][0]];
        const glm::vec2& b = m.P[m.T[t][1]];
        const glm::vec2& c = m.P[m.T[t][2]];
        const float ba = back[m.T[t][0]];
        const float bb = back[m.T[t][1]];
        const float bc = back[m.T[t][2]];

        bool bad = false;
        ForEachPixelInTriangle(a, b, c, W, H, [&](int x, int y, float la, float lb, float lc)
        {
            if (bad)
                return;
            const float d1 = D1.At(x, y);
            const float d2 = D2.At(x, y);
            if (!IsSurface(d1) || !IsSurface(d2))
                return;
            // The second layer lies strictly behind the first; it is "inside" the prism when it is
            // in front of the (interpolated) back face.
            if (d2 < la * ba + lb * bb + lc * bc)
                bad = true;
        });

        if (bad)
            flagged.push_back(static_cast<int>(t));
    }
    return flagged;
}

glm::vec3 Unproject(const glm::mat4& invViewProj, float x, float y, float depth, int W, int H)
{
    const glm::vec4 ndc(2.0f * x / static_cast<float>(W) - 1.0f,
                        2.0f * y / static_cast<float>(H) - 1.0f,
                        2.0f * depth - 1.0f,
                        1.0f);
    const glm::vec4 w = invViewProj * ndc;
    return glm::vec3(w) / w.w;
}

} // namespace

// =============================================================================================
// Public API
// =============================================================================================

bool TriangulatePolygon(std::vector<glm::vec2> poly,
                        std::vector<glm::vec2>& outVerts,
                        std::vector<uint32_t>& outTris,
                        std::string& error)
{
    outVerts.clear();
    outTris.clear();

    // Drop consecutive duplicates (and a repeated closing point).
    std::vector<glm::vec2> p;
    for (size_t i = 0; i < poly.size(); ++i)
    {
        if (p.empty() || glm::distance(poly[i], p.back()) > 0.5f)
            p.push_back(poly[i]);
    }
    while (p.size() > 1 && glm::distance(p.front(), p.back()) <= 0.5f)
        p.pop_back();

    if (p.size() < 3)
    {
        error = "the outline needs at least 3 distinct points";
        return false;
    }

    if (SelfIntersects(p))
    {
        error = "the outline crosses itself in the current view; annotate from a view where it does not";
        return false;
    }
    const float area2 = SignedArea2(p);
    if (std::fabs(area2) < 4.0f)
    {
        error = "the outline encloses (almost) no area in the current view";
        return false;
    }
    if (area2 < 0.0f)
        std::reverse(p.begin(), p.end());

    std::vector<int> idx(p.size());
    std::iota(idx.begin(), idx.end(), 0);

    size_t guard = 0;
    const size_t maxIter = p.size() * p.size() + 16;

    while (idx.size() > 3)
    {
        if (++guard > maxIter)
        {
            error = "triangulation failed (degenerate outline)";
            return false;
        }

        const int m = static_cast<int>(idx.size());
        bool clipped = false;

        for (int i = 0; i < m && !clipped; ++i)
        {
            const int ia = idx[(i + m - 1) % m];
            const int ib = idx[i];
            const int ic = idx[(i + 1) % m];
            const glm::vec2& a = p[ia];
            const glm::vec2& b = p[ib];
            const glm::vec2& c = p[ic];

            if (Cross2(b - a, c - b) <= 1e-4f)   // reflex or collinear corner: not an ear
                continue;

            bool blocked = false;
            for (int k = 0; k < m && !blocked; ++k)
            {
                const int ik = idx[k];
                if (ik == ia || ik == ib || ik == ic)
                    continue;
                blocked = PointInTriangle(p[ik], a, b, c);
            }
            if (blocked)
                continue;

            outTris.push_back(static_cast<uint32_t>(ia));
            outTris.push_back(static_cast<uint32_t>(ib));
            outTris.push_back(static_cast<uint32_t>(ic));
            idx.erase(idx.begin() + i);
            clipped = true;
        }

        if (!clipped)
        {
            // Only (nearly) collinear corners are left: drop the flattest one without emitting a triangle.
            int best = -1;
            float bestAbs = kBig;
            for (int i = 0; i < m; ++i)
            {
                const glm::vec2& a = p[idx[(i + m - 1) % m]];
                const glm::vec2& b = p[idx[i]];
                const glm::vec2& c = p[idx[(i + 1) % m]];
                const float cr = std::fabs(Cross2(b - a, c - b));
                if (cr < bestAbs)
                {
                    bestAbs = cr;
                    best = i;
                }
            }
            if (best < 0 || bestAbs > 1.0f)
            {
                error = "triangulation failed (degenerate outline)";
                return false;
            }
            idx.erase(idx.begin() + best);
        }
    }

    if (idx.size() == 3)
    {
        const glm::vec2& a = p[idx[0]];
        const glm::vec2& b = p[idx[1]];
        const glm::vec2& c = p[idx[2]];
        if (Cross2(b - a, c - b) > 1e-4f)
        {
            outTris.push_back(static_cast<uint32_t>(idx[0]));
            outTris.push_back(static_cast<uint32_t>(idx[1]));
            outTris.push_back(static_cast<uint32_t>(idx[2]));
        }
    }

    if (outTris.empty())
    {
        error = "triangulation produced no triangles";
        return false;
    }

    outVerts = p;
    return true;
}

float SignedVolume(const std::vector<glm::vec3>& p, const std::vector<uint32_t>& idx)
{
    double v = 0.0;
    for (size_t i = 0; i + 2 < idx.size(); i += 3)
    {
        const glm::dvec3 a(p[idx[i]]);
        const glm::dvec3 b(p[idx[i + 1]]);
        const glm::dvec3 c(p[idx[i + 2]]);
        v += glm::dot(a, glm::cross(b, c));
    }
    return static_cast<float>(v / 6.0);
}

bool CheckWatertight(const std::vector<uint32_t>& idx, std::string* why)
{
    // Closed and consistently oriented <=> every directed edge occurs exactly once and so does its reverse.
    std::unordered_map<uint64_t, int> edges;
    for (size_t i = 0; i + 2 < idx.size(); i += 3)
    {
        for (int k = 0; k < 3; ++k)
        {
            const uint64_t u = idx[i + k];
            const uint64_t v = idx[i + (k + 1) % 3];
            ++edges[(u << 32) | v];
        }
    }
    for (std::unordered_map<uint64_t, int>::const_iterator it = edges.begin(); it != edges.end(); ++it)
    {
        const uint64_t u = it->first >> 32;
        const uint64_t v = it->first & 0xffffffffull;
        std::unordered_map<uint64_t, int>::const_iterator rev = edges.find((v << 32) | u);
        if (it->second != 1 || rev == edges.end() || rev->second != 1)
        {
            if (why)
                *why = "edge " + std::to_string(u) + "->" + std::to_string(v) + " is not matched by exactly one opposite edge";
            return false;
        }
    }
    return true;
}

BuildResult BuildClippingVolume(const BuildInput& in, const BuildParams& params)
{
    BuildResult res;

    if (!in.layer1 || !in.layer1->Valid())
    {
        res.error = "no depth map available";
        return res;
    }
    const DepthMap& D1 = *in.layer1;
    const int W = D1.width;
    const int H = D1.height;
    const DepthMap* D2 = (in.layer2 && in.layer2->Valid() && in.layer2->width == W && in.layer2->height == H)
                             ? in.layer2 : nullptr;

    // Steps 1-2: planar triangulation of the outline in view space.
    std::vector<glm::vec2> verts;
    std::vector<uint32_t> tris;
    if (!TriangulatePolygon(in.polygonPx, verts, tris, res.error))
        return res;

    Mesh2D m;
    m.P = verts;
    for (size_t i = 0; i + 2 < tris.size(); i += 3)
    {
        std::array<uint32_t, 3> t = { tris[i], tris[i + 1], tris[i + 2] };
        m.T.push_back(t);
        m.alive.push_back(1);
    }

    // Steps 3-4: offsets, refined where a prism still swallows a second surface layer.
    std::vector<float> dmin, dmax;
    BuildMinMax3x3(D1, dmin, dmax);

    std::vector<float> front, back;
    for (int round = 0;; ++round)
    {
        if (!ComputeOffsets(m, D1, dmin, dmax, params.depthMargin, front, back))
        {
            res.error = "the outline does not cover the model surface";
            return res;
        }
        if (!params.refineThinFeatures || !D2 || round >= params.maxRefineRounds)
            break;
        if (m.AliveCount() > params.maxTriangles)
            break;

        const std::vector<int> flagged = FindContaminated(m, D1, *D2, back, params.minTriangleAreaPx);
        if (flagged.empty())
            break;

        for (size_t i = 0; i < flagged.size(); ++i)
        {
            if (m.alive[flagged[i]])
                m.Refine(flagged[i]);
        }
        ++res.refineRounds;
    }

    // Compact: keep only vertices that are referenced by a live triangle.
    std::vector<int> remap(m.P.size(), -1);
    int N = 0;
    for (size_t t = 0; t < m.T.size(); ++t)
    {
        if (!m.alive[t])
            continue;
        for (int k = 0; k < 3; ++k)
        {
            if (remap[m.T[t][k]] < 0)
            {
                remap[m.T[t][k]] = N++;
                res.px.push_back(m.P[m.T[t][k]]);
                res.front.push_back(front[m.T[t][k]]);
                res.back.push_back(back[m.T[t][k]]);
            }
        }
        res.tris2D.push_back(static_cast<uint32_t>(remap[m.T[t][0]]));
        res.tris2D.push_back(static_cast<uint32_t>(remap[m.T[t][1]]));
        res.tris2D.push_back(static_cast<uint32_t>(remap[m.T[t][2]]));
    }

    // Step 5: extrude along the view rays. Front cap faces the camera, back cap is mirrored, and the
    // side walls follow the boundary edges of the (refined) triangulation.
    const glm::mat4 inv = glm::inverse(in.viewProj);
    res.positions.resize(static_cast<size_t>(2 * N));
    for (int i = 0; i < N; ++i)
    {
        res.positions[static_cast<size_t>(i)] = Unproject(inv, res.px[i].x, res.px[i].y, res.front[i], W, H);
        res.positions[static_cast<size_t>(N + i)] = Unproject(inv, res.px[i].x, res.px[i].y, res.back[i], W, H);
    }

    std::unordered_map<uint64_t, int> directed;
    for (size_t i = 0; i + 2 < res.tris2D.size(); i += 3)
    {
        const uint32_t a = res.tris2D[i], b = res.tris2D[i + 1], c = res.tris2D[i + 2];
        res.indices.push_back(a);
        res.indices.push_back(b);
        res.indices.push_back(c);
        res.indices.push_back(static_cast<uint32_t>(N) + a);
        res.indices.push_back(static_cast<uint32_t>(N) + c);
        res.indices.push_back(static_cast<uint32_t>(N) + b);

        for (int k = 0; k < 3; ++k)
        {
            const uint64_t u = res.tris2D[i + k];
            const uint64_t v = res.tris2D[i + (k + 1) % 3];
            directed[(u << 32) | v] = 1;
        }
    }

    for (std::unordered_map<uint64_t, int>::const_iterator it = directed.begin(); it != directed.end(); ++it)
    {
        const uint64_t u = it->first >> 32;
        const uint64_t v = it->first & 0xffffffffull;
        if (directed.find((v << 32) | u) != directed.end())
            continue;   // interior edge

        const uint32_t fu = static_cast<uint32_t>(u), fv = static_cast<uint32_t>(v);
        const uint32_t bu = fu + static_cast<uint32_t>(N), bv = fv + static_cast<uint32_t>(N);
        res.indices.push_back(fv); res.indices.push_back(fu); res.indices.push_back(bu);
        res.indices.push_back(fv); res.indices.push_back(bu); res.indices.push_back(bv);
    }

    // The window->world map may mirror the orientation; make the winding outward (positive volume).
    const float vol = SignedVolume(res.positions, res.indices);
    if (std::fabs(vol) < 1e-12f)
    {
        res.error = "the clipping volume is degenerate";
        res.indices.clear();
        res.positions.clear();
        return res;
    }
    if (vol < 0.0f)
    {
        for (size_t i = 0; i + 2 < res.indices.size(); i += 3)
            std::swap(res.indices[i + 1], res.indices[i + 2]);
    }

    res.ok = true;
    return res;
}

float ApproximateEnclosedArea(const BuildResult& volume,
                              const glm::mat4& viewProj,
                              int W, int H,
                              const std::vector<glm::vec3>& meshPositions,
                              const std::vector<uint32_t>& meshIndices)
{
    if (!volume.ok || volume.px.empty() || W <= 0 || H <= 0)
        return 0.0f;

    // Per-pixel front/back range of the prism.
    std::vector<float> fr(static_cast<size_t>(W) * H, kBig);
    std::vector<float> bk(static_cast<size_t>(W) * H, -kBig);

    for (size_t i = 0; i + 2 < volume.tris2D.size(); i += 3)
    {
        const uint32_t ia = volume.tris2D[i], ib = volume.tris2D[i + 1], ic = volume.tris2D[i + 2];
        ForEachPixelInTriangle(volume.px[ia], volume.px[ib], volume.px[ic], W, H,
            [&](int x, int y, float la, float lb, float lc)
            {
                const size_t p = static_cast<size_t>(y) * W + x;
                const float f = la * volume.front[ia] + lb * volume.front[ib] + lc * volume.front[ic];
                const float b = la * volume.back[ia] + lb * volume.back[ib] + lc * volume.back[ic];
                fr[p] = (std::min)(fr[p], f);
                bk[p] = (std::max)(bk[p], b);
            });
    }

    double area = 0.0;
    for (size_t i = 0; i + 2 < meshIndices.size(); i += 3)
    {
        const glm::vec3& p0 = meshPositions[meshIndices[i]];
        const glm::vec3& p1 = meshPositions[meshIndices[i + 1]];
        const glm::vec3& p2 = meshPositions[meshIndices[i + 2]];
        const glm::vec3 centroid = (p0 + p1 + p2) / 3.0f;

        const glm::vec4 clip = viewProj * glm::vec4(centroid, 1.0f);
        if (clip.w <= 0.0f)
            continue;
        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        const int x = static_cast<int>(std::floor((ndc.x * 0.5f + 0.5f) * static_cast<float>(W)));
        const int y = static_cast<int>(std::floor((ndc.y * 0.5f + 0.5f) * static_cast<float>(H)));
        if (x < 0 || y < 0 || x >= W || y >= H)
            continue;

        const float d = ndc.z * 0.5f + 0.5f;
        const size_t p = static_cast<size_t>(y) * W + x;
        if (fr[p] <= d && d <= bk[p])
            area += 0.5 * glm::length(glm::cross(p1 - p0, p2 - p0));
    }
    return static_cast<float>(area);
}

} // namespace clipvol
