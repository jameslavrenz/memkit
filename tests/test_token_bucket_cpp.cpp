#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

int main()
{
    memkit::TokenBucket limiter;
    assert(memkit::ok(limiter.init(10u, 2u)));
    assert(limiter.tokens() == 10u);

    assert(memkit::ok(limiter.try_consume(4u)));
    assert(limiter.tokens() == 6u);
    assert(limiter.try_consume(8u) == memkit::status::empty);

    limiter.refill(2u);
    assert(limiter.tokens() == 10u);

    limiter.reset();
    assert(limiter.tokens() == 10u);
    assert(memkit::ok(limiter.consume(1u)));

    std::printf("token_bucket: ok\n");
    return 0;
}
