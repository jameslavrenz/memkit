#include <memkit/memkit.hpp>

volatile int sink = 0;

int main()
{
    memkit::stl::array<int, 128> storage{};
    memkit::Ring<int> ring;
    ring.init(storage);

    for (int i = 0; i < 4096; ++i) {
        (void)ring.push_back(i);
        int out = 0;
        (void)ring.pop_front(&out);
        sink += out;
    }

    return sink == 0 ? 0 : 0;
}
