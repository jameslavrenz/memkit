#include <memkit/memkit.hpp>

volatile int sink = 0;

int main()
{
    memkit::stl::array<int, 128> storage{};
    memkit::Queue<int> queue;
    (void)queue.init(storage);

    for (int i = 0; i < 4096; ++i) {
        (void)queue.push_back(i);
        int out = 0;
        (void)queue.pop_front(&out);
        sink += out;
    }

    return 0;
}
