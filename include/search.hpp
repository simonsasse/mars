#pragma once

#include <cmath>
#include <cstdint>

#include "motif.hpp"
#include "index.hpp"
#include "settings.hpp"

namespace mars
{

struct Hit
{
    MotifNum id;
    SeqNum seq;
    SeqLen pos;
    MotifScore score;

    Hit(MotifNum id, SeqNum seq, SeqLen pos, MotifScore score) :
        id{id}, seq{seq}, pos{pos}, score{score}
    {}
};

struct BackgroundDistribution
{
    std::array<MotifScore, 16> const r16
    {
        // Source:
        // Olson, W. K., Esguerra, M., Xin, Y., & Lu, X. J. (2009).
        // New information content in RNA base pairing deduced from quantitative analysis of high-resolution structures.
        // Methods (San Diego, Calif.), 47(3), 177–186. https://doi.org/10.1016/j.ymeth.2008.12.003

        log2( 384.f     / 17328), // AA
        log2( 313.f / 2 / 17328), // AC
        log2( 980.f / 2 / 17328), // AG
        log2(3975.f / 2 / 17328), // AU
        log2( 313.f / 2 / 17328), // CA
        log2(  63.f     / 17328), // CC
        log2(9913.f / 2 / 17328), // CG
        log2( 103.f / 2 / 17328), // CU
        log2( 980.f / 2 / 17328), // GA
        log2(9913.f / 2 / 17328), // GC
        log2( 128.f     / 17328), // GG
        log2(1282.f / 2 / 17328), // GU
        log2(3975.f / 2 / 17328), // UA
        log2( 103.f / 2 / 17328), // UC
        log2(1282.f / 2 / 17328), // UG
        log2( 187.f     / 17328)  // UU
    };

    std::array<MotifScore, 4> const r4
    {
        log2(0.3f), // A
        log2(0.2f), // C
        log2(0.2f), // G
        log2(0.3f)  // U
    };

    template <unsigned short size> requires (size == 4 || size == 16)
    std::array<MotifScore, size> const & get() const
    {
        if constexpr (size == 4)
            return r4;
        else
            return r16;
    }
};

class SearchGenerator
{
private:
    using ElementIter = typename std::vector<std::variant<LoopElement, StemElement>>::const_reverse_iterator;

    BiDirectionalSearch bds;
    std::vector<Hit> hits;
    MotifScore const log_depth;
    ElementIter end;
    BackgroundDistribution const background_distr;

    template <typename MotifElement>
    void recurse_search(ElementIter const & it, MotifLen idx);

    template <seqan3::semialphabet Alphabet>
    inline std::set<std::pair<MotifScore, Alphabet>> priority(profile_char<Alphabet> const & prof) const;

public:
    SearchGenerator(Index index, SeqNum depth, unsigned char xdrop) :
        bds{std::move(index), std::move(xdrop)},
        hits{},
        log_depth{log2f(depth)},
        background_distr{}
    {}

    void find_motif(StemloopMotif const & motif);
};

} // namespace mars
