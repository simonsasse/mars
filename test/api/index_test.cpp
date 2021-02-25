#include <gtest/gtest.h>

#include <seqan3/alphabet/nucleotide/rna4.hpp>

#include "index.hpp"

// Generate the full path of a test input file that is provided in the data directory.
std::filesystem::path data(std::string const & filename)
{
    return std::filesystem::path{std::string{DATADIR}}.concat(filename);
}

TEST(Index, Create)
{
    // from fasta file
    mars::BiDirectionalIndex bds(4);
    EXPECT_NO_THROW(bds.create(data("genome.fa")));
#ifdef SEQAN3_HAS_ZLIB
    std::filesystem::path const indexfile = data("genome.fa.marsindex.gz");
#else
    std::filesystem::path const indexfile = data("genome.fa.marsindex");
#endif
    EXPECT_TRUE(std::filesystem::exists(indexfile));
    std::filesystem::remove(indexfile);

    // from archive
    EXPECT_NO_THROW(bds.create(data("genome2.fa")));

    // from compressed archive
#ifdef SEQAN3_HAS_ZLIB
    EXPECT_NO_THROW(bds.create(data("genome3.fa")));
#endif
}

TEST(Index, BiDirectionalIndex)
{
    using seqan3::operator""_rna4;

    mars::BiDirectionalIndex bds(4);
    bds.create(data("RF0005.fa"));
    mars::bi_alphabet ba{'U'_rna4, 'C'_rna4};

    bds.append_loop({0.f, 'A'_rna4}, false);
    bds.append_stem({0.f, ba});
    bds.append_loop({0.f, 'A'_rna4}, true);
    bds.append_loop({0.f, 'G'_rna4}, false);
    bds.append_loop({0.f, 'A'_rna4}, false);
    bds.append_loop({0.f, 'A'_rna4}, false);
    bds.append_stem({0.f, ba});
    bds.backtrack();
    bds.append_loop({0.f, 'A'_rna4}, false);
    bds.append_loop({0.f, 'G'_rna4}, false);
    bds.append_loop({0.f, 'G'_rna4}, false);
    bds.append_loop({0.f, 'G'_rna4}, false);

    size_t num_matches = bds.compute_matches();
    EXPECT_EQ(num_matches, 0ul);

    bds.backtrack();
    num_matches = bds.compute_matches();
    EXPECT_EQ(num_matches, 5ul);
}
