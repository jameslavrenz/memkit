#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

struct msg_a {
    int value = 0;
};

struct msg_b {
    float value = 0.0f;
};

int main()
{
    memkit::FixedVariant<int, msg_a, msg_b> message;
    assert(message.empty());

    assert(memkit::ok(message.emplace<int>(42)));
    assert(message.holds<int>());

    int iv = 0;
    assert(memkit::ok(message.get(iv)));
    assert(iv == 42);

    assert(memkit::ok(message.set(msg_a{100})));
    assert(message.holds<msg_a>());
    const msg_a* ap = message.try_get<msg_a>();
    assert(ap != nullptr);
    assert(ap->value == 100);

    assert(memkit::ok(message.visit([](const auto& alt) {
        using alt_t = std::decay_t<decltype(alt)>;
        if constexpr (std::is_same_v<alt_t, msg_a>) {
            assert(alt.value == 100);
        } else {
            assert(false);
        }
        return memkit::status::ok;
    })));

    memkit::FixedVariant<int, msg_a, msg_b> moved = std::move(message);
    assert(moved.holds<msg_a>());
    assert(message.empty());

    moved.clear();
    assert(moved.empty());

    std::printf("fixed_variant: ok\n");
    return 0;
}
