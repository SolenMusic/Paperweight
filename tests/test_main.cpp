#include <paperweight/version.hpp>

#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void expect(bool condition, std::string_view description)
{
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

void testVersion()
{
    constexpr paperweight::Version expected{0, 0, 1};
    static_assert(paperweight::currentVersion == expected);
    expect(paperweight::versionString() == "0.0.1", "version string is 0.0.1");
}

} // namespace

int main()
{
    testVersion();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "All Paperweight tests passed\n";
    return 0;
}
