#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

int main()
{
    memkit::SmallString<16> name{};
    assert(memkit::ok(name.assign("sensor-1")));
    assert(name.size() == 8u);
    assert(name.view() == "sensor-1");
    assert(memkit::ok(name.append("-ok")));
    assert(name.view() == "sensor-1-ok");

    memkit::SmallString<4> tiny{};
    assert(tiny.assign("abcd") == memkit::status::ok);
    assert(tiny.append("e") == memkit::status::full);
    assert(tiny.assign("way-too-long-string") == memkit::status::invalid);

    tiny.clear();
    assert(tiny.empty());
    assert(tiny.c_str()[0] == '\0');

    std::printf("small_string: ok\n");
    return 0;
}
