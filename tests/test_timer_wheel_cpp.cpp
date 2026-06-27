#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

struct timer_ctx {
    int fired_count = 0;
    int last_id     = 0;
};

static void on_timer_a(void* user)
{
    auto* ctx = static_cast<timer_ctx*>(user);
    ++ctx->fired_count;
    ctx->last_id = 1;
}

static void on_timer_b(void* user)
{
    auto* ctx = static_cast<timer_ctx*>(user);
    ++ctx->fired_count;
    ctx->last_id = 2;
}

int main()
{
    memkit::TimerWheel<8> wheel;
    assert(memkit::ok(wheel.init()));

    timer_ctx ctx{};
    memkit::TimerWheelNode node_a{};
    node_a.callback = on_timer_a;
    node_a.user     = &ctx;

    memkit::TimerWheelNode node_b{};
    node_b.callback = on_timer_b;
    node_b.user     = &ctx;

    assert(memkit::ok(wheel.schedule(node_a, 3u)));
    assert(memkit::ok(wheel.schedule(node_b, 8u)));

    assert(memkit::ok(wheel.tick(2u)));
    assert(ctx.fired_count == 0);

    assert(memkit::ok(wheel.tick(1u)));
    assert(ctx.fired_count == 1);
    assert(ctx.last_id == 1);

    assert(memkit::ok(wheel.tick(4u)));
    assert(ctx.fired_count == 1);

    assert(memkit::ok(wheel.tick(1u)));
    assert(ctx.fired_count == 2);
    assert(ctx.last_id == 2);

    memkit::TimerWheelNode node_c{};
    node_c.callback = on_timer_a;
    node_c.user     = &ctx;
    assert(memkit::ok(wheel.schedule(node_c, 5u)));
    wheel.cancel(node_c);
    assert(!node_c.hook.is_linked());

    assert(memkit::ok(wheel.tick(10u)));
    assert(ctx.fired_count == 2);

    std::printf("timer_wheel: ok\n");
    return 0;
}
