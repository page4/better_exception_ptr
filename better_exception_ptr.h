#pragma once

#include "abi_details.h"

#include <exception>

namespace stdx {
namespace detail {
struct current_exception_tag {};

struct catch_all_tag {};

template <typename Class>
struct ArgumentImpl : public ArgumentImpl<decltype(&Class::operator())> {};

template <typename Class, typename Ret, typename... Args>
struct ArgumentImpl<Ret (Class::*)(Args..., ...)> : public ArgumentImpl<Ret(Args..., ...)> {};
template <typename Class, typename Ret, typename... Args>
struct ArgumentImpl<Ret (Class::*)(Args..., ...) const> : public ArgumentImpl<Ret(Args..., ...)> {};
template <typename Class, typename Ret, typename... Args>
struct ArgumentImpl<Ret (Class::*)(Args...)> : public ArgumentImpl<Ret(Args...)> {};
template <typename Class, typename Ret, typename... Args>
struct ArgumentImpl<Ret (Class::*)(Args...) const> : public ArgumentImpl<Ret(Args...)> {};

template <typename Ret, typename Arg>
struct ArgumentImpl<Ret(Arg)> {
    using type = Arg;
};

template <typename Ret>
struct ArgumentImpl<Ret(...)> {
    using type = catch_all_tag;
};

template <typename Ret>
struct ArgumentImpl<Ret()> {
    using type = void;
};

template <typename T>
using ArgumentOf = typename ArgumentImpl<T>::type;

template <typename T, typename Argument = ArgumentOf<T>>
struct ReturnOfImpl : public std::result_of<T(ArgumentOf<T>)> {};

template <typename T>
struct ReturnOfImpl<T, void> : public std::result_of<T()> {};

template <typename T>
using ReturnOf = typename ReturnOfImpl<T>::type;
}

class exception_ptr : public std::exception_ptr {
public:
    /*implicit*/ exception_ptr(const std::exception_ptr& ex) : std::exception_ptr(ex) {}
    /*implicit*/ exception_ptr(std::exception_ptr&& ex) : std::exception_ptr(std::move(ex)) {}

    //
    // High level api
    //

    bool handle() {
        return false;
    }

    template <
        typename Handler,
        typename... Rest,
        typename CommonType = 
            std::common_type_t<detail::ReturnOf<Handler>, detail::ReturnOf<Rest>...>
        >
    std::conditional_t<std::is_void_v<CommonType>, bool, std::optional<CommonType>>
    handle(Handler&& handler, Rest&&... rest) {
        using Argument = detail::ArgumentOf<std::decay_t<Handler>>;
        static_assert(!std::is_same<Argument, detail::catch_all_tag>()); // TODO

        if (auto caught = try_catch<Argument>()) {
            if constexpr(std::is_void_v<CommonType>) {
                handler(*caught);
                return true;
            } else {
                return {handler(*caught)};
            }
        }

        if constexpr(sizeof...(Rest) == 0)
            return {};

        return handle(std::forward<Rest>(rest)...);
    }

    [[noreturn]] void handle_or_terminate() {
        terminate_with_active();
    }

    template <typename Handler, typename... Rest>
    std::common_type_t<detail::ReturnOf<Handler>, detail::ReturnOf<Rest>...>
    handle_or_terminate(Handler&& handler, Rest&&... rest) {
        using Argument = detail::ArgumentOf<std::decay_t<Handler>>;
        static_assert(!std::is_same<Argument, detail::catch_all_tag>()); // TODO
        if (auto caught = try_catch<Argument>()) {
            return handler(*caught);
        }
        return handle_or_terminate(std::forward<Rest>(rest)...);
    }

    template <typename T>
    std::conditional_t<
        std::is_pointer_v<T>,
        std::optional<T>,
        std::add_pointer_t<T>>
    try_catch() const {
        static_assert(std::is_reference_v<T> || std::is_pointer_v<T>);

        using target = 
            std::conditional_t<
                std::is_pointer_v<T>,
                std::optional<T>,
                std::add_pointer_t<T>>;

        if (auto raw = detail::try_catch(*this, &typeid(T)))
            return static_cast<target>(*raw);
        return {};
    }

    [[noreturn]] void terminate_with_active() const noexcept {
        try {
            std::rethrow_exception(*this);
        } catch (...) {
            std::terminate();
        }
    }

    //
    // Low level api
    //

    std::type_info* type() const {
        return detail::type(*this);
    }

    void* get_raw_ptr() const {
        return detail::get_raw_ptr(*this);
    }

    exception_ptr(detail::current_exception_tag) : std::exception_ptr(std::current_exception()) {}
};

exception_ptr current_exception() {
    return exception_ptr(detail::current_exception_tag());
}

using std::rethrow_exception;
}