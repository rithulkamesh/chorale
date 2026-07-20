#pragma once

#include <algorithm>
#include <vector>

// Voice-side signal graph: a fixed pool of nodes wired by an edge list.
// Inputs sum implicitly (every input port is a summing junction), so there is
// no explicit Sum node; GAIN nodes are leveled summers. JUCE-free.
namespace graph
{
// Fixed node ids. The pool never grows or shrinks; a node is "in use" when an
// edge touches it. FX node k maps 1:1 onto voice k+1's parameter set
// (v<k+1>Eq*, v<k+1>Comp*, v<k+1>Sat*), GAIN k onto gain<k+1>Level, ECHO k
// onto echo<k+1>*, VERB k onto verb<k+1>*. New kinds append after kOut so the
// serialized ids of older patches never shift.
constexpr int kVoice0 = 0; // 0..7   sources (post level/pan/humanize)
constexpr int kEq0 = 8;    // 8..15
constexpr int kComp0 = 16; // 16..23
constexpr int kSat0 = 24;  // 24..31
constexpr int kGain0 = 32; // 32..35
constexpr int kOut = 36;   // wet bus in
constexpr int kEcho0 = 37; // 37..38
constexpr int kVerb0 = 39; // 39..40
constexpr int kNumNodes = 41;

enum class NodeKind { Voice, Eq, Comp, Sat, Gain, Out, Echo, Verb };

inline NodeKind kindOf (int id)
{
    if (id < kEq0) return NodeKind::Voice;
    if (id < kComp0) return NodeKind::Eq;
    if (id < kSat0) return NodeKind::Comp;
    if (id < kGain0) return NodeKind::Sat;
    if (id < kOut) return NodeKind::Gain;
    if (id == kOut) return NodeKind::Out;
    if (id < kVerb0) return NodeKind::Echo;
    return NodeKind::Verb;
}

struct Edge
{
    int from = 0, to = 0;
    bool operator== (const Edge& o) const { return from == o.from && to == o.to; }
};

struct Plan
{
    bool valid = false;
    std::vector<int> order;               // non-source nodes, topological
    std::vector<int> inputs[kNumNodes];   // per node: source node ids
    int stemTap[8] = { 0, 1, 2, 3, 4, 5, 6, 7 }; // end of each voice's lane
};

// Kahn topological sort over the used subgraph. Cycles -> plan.valid = false.
inline Plan compile (const std::vector<Edge>& edges)
{
    Plan p;
    int indeg[kNumNodes] = {};
    int outdeg[kNumNodes] = {};
    std::vector<int> adj[kNumNodes];
    for (const auto& e : edges)
    {
        if (e.from < 0 || e.from >= kNumNodes || e.to < 0 || e.to >= kNumNodes)
            return p;
        if (kindOf (e.to) == NodeKind::Voice || kindOf (e.from) == NodeKind::Out)
            return p; // sources have no inputs, OUT has no outputs
        p.inputs[e.to].push_back (e.from);
        adj[e.from].push_back (e.to);
        ++indeg[e.to];
        ++outdeg[e.from];
    }

    std::vector<int> ready;
    for (int n = 0; n < kNumNodes; ++n)
        if (indeg[n] == 0)
            ready.push_back (n);
    int remaining[kNumNodes];
    std::copy (indeg, indeg + kNumNodes, remaining);

    std::vector<int> topo;
    while (! ready.empty())
    {
        const int n = ready.back();
        ready.pop_back();
        topo.push_back (n);
        for (int m : adj[n])
            if (--remaining[m] == 0)
                ready.push_back (m);
    }
    if ((int) topo.size() != kNumNodes)
        return p; // cycle

    for (int n : topo)
        if (kindOf (n) != NodeKind::Voice && ! p.inputs[n].empty())
            p.order.push_back (n);

    // Stem/send tap: follow each voice's private lane (single out, single in,
    // FX nodes only) to its end, so stems and sends stay post-FX like the
    // fixed chains were.
    for (int v = 0; v < 8; ++v)
    {
        int cur = kVoice0 + v;
        for (;;)
        {
            if (outdeg[cur] != 1)
                break;
            int next = -1;
            for (const auto& e : edges)
                if (e.from == cur)
                {
                    next = e.to;
                    break;
                }
            if (next < 0 || next == kOut || kindOf (next) == NodeKind::Voice
                || (int) p.inputs[next].size() != 1)
                break;
            cur = next;
        }
        p.stemTap[v] = cur;
    }

    p.valid = true;
    return p;
}

// The default patch: voices straight into OUT. With every FX module bypassed
// by default this is audibly identical to the old fixed chains, and the
// canvas starts clean — FX nodes appear only when the user adds them.
inline std::vector<Edge> defaultEdges()
{
    std::vector<Edge> e;
    for (int v = 0; v < 8; ++v)
        e.push_back ({ kVoice0 + v, kOut });
    return e;
}

// The classic fixed chains (Voice v -> EQ v -> COMP v -> SAT v -> OUT);
// used by tests and available as a starting point.
inline std::vector<Edge> chainEdges()
{
    std::vector<Edge> e;
    for (int v = 0; v < 8; ++v)
    {
        e.push_back ({ kVoice0 + v, kEq0 + v });
        e.push_back ({ kEq0 + v, kComp0 + v });
        e.push_back ({ kComp0 + v, kSat0 + v });
        e.push_back ({ kSat0 + v, kOut });
    }
    return e;
}
} // namespace graph
