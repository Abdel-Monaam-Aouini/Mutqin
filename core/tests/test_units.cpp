#include <gtest/gtest.h>

#include <fstream>
#include <string>

#include "inference/units.h"

using namespace mutqin;

TEST(Units, CollapseRunsCutsElongationByCodePoint) {
    EXPECT_EQ(CollapseRuns("ااااا", 2), "اا");
    EXPECT_EQ(CollapseRuns("ممممِ", 2), "ممِ");
    EXPECT_EQ(CollapseRuns("بَ", 2), "بَ");          // nothing to cut
    EXPECT_EQ(CollapseRuns("ررَ", 2), "ررَ");        // a geminate (two letters) stays
    EXPECT_EQ(CollapseRuns("ااااا", 0), "ااااا");  // off
    EXPECT_EQ(CollapseRuns("", 2), "");
}

TEST(Units, LoadsTheGroupsOfARange) {
    const std::string path = ::testing::TempDir() + "/units_test.tsv";
    {
        std::ofstream out(path);
        out << "1:1\t1-1\ta b\n1:1\t2-3\tc d e\n1:2\t1-1\tf\n2:1\t1-1\tz\n";
    }
    std::vector<UnitGroup> groups;
    std::string error;
    ASSERT_TRUE(LoadUnitGroups(path, 1, 1, 2, &groups, &error)) << error;
    ASSERT_EQ(groups.size(), 3u);
    EXPECT_EQ(groups[1].first_word, 2);
    EXPECT_EQ(groups[1].last_word, 3);
    EXPECT_EQ(groups[1].units, (std::vector<std::string>{"c", "d", "e"}));
    EXPECT_FALSE(LoadUnitGroups(path, 1, 1, 3, &groups, &error));  // 1:3 is missing
    EXPECT_NE(error.find("1:3"), std::string::npos);
    EXPECT_FALSE(LoadUnitGroups(path + ".missing", 1, 1, 1, &groups, &error));
}
