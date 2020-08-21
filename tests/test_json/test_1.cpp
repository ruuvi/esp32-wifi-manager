#include "gtest/gtest.h"
#include "json.h"

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestJson : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
    }

    void
    TearDown() override
    {
    }

public:
    TestJson();

    ~TestJson() override;
};

TestJson::TestJson()
    : Test()
{
}

TestJson::~TestJson()
{
}

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestJson, test_1) // NOLINT
{
    ASSERT_EQ(false, json_print_string(nullptr, nullptr));
}
