#include <cassert>
#include <cstdio>

#include <memkit/memkit.hpp>

struct task_node {
    memkit::IntrusiveListHook hook{};
    int                       id = 0;
};

int main()
{
    memkit::IntrusiveListHead list;

    task_node a{ .id = 1 };
    task_node b{ .id = 2 };
    task_node c{ .id = 3 };

    list.push_back(a.hook);
    list.push_back(b.hook);
    list.push_front(c.hook);

    assert(!list.empty());
    assert(list.front()->next != nullptr);

    int sum = 0;
    while (!list.empty()) {
        memkit::IntrusiveListHook* hook = list.front();
        task_node* node = memkit::container_from_hook<task_node, memkit::IntrusiveListHook, &task_node::hook>(
            hook
        );
        sum += node->id;
        list.erase(*hook);
    }

    assert(sum == 6);
    assert(list.empty());

    memkit::IntrusiveForwardListHead flist;
    struct fwd_node {
        memkit::IntrusiveForwardHook hook{};
        int                          value = 0;
    };

    fwd_node x{ .value = 10 };
    fwd_node y{ .value = 20 };
    flist.push_front(x.hook);
    flist.push_front(y.hook);
    assert(flist.front()->next != nullptr);
    flist.pop_front();
    assert(flist.front()->next == nullptr);

    std::printf("intrusive_list: ok\n");
    return 0;
}
