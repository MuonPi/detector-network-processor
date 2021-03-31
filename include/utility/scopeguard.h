#ifndef SCOPEGUARD_H
#define SCOPEGUARD_H

#include <functional>

namespace muonpi {

class scope_guard {
public:
    template <class Func>
    scope_guard(const Func& cleanup);

    scope_guard(scope_guard&& other);

    scope_guard() = delete;
    scope_guard(const scope_guard&) = delete;

    auto operator=(scope_guard&&) -> scope_guard& = delete;
    auto operator=(const scope_guard&) -> scope_guard& = delete;

    ~scope_guard();

    void dismiss();

private:
    [[nodiscard]] auto dissolve() -> std::function<void()>;

    std::function<void()> m_cleanup;
};

template <class Func>
scope_guard::scope_guard(const Func& cleanup)
    : m_cleanup { cleanup }
{
}

}

#endif // SCOPEGUARD_H
