module;

#include <cctype>
#include <format>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>

export module utilities;

export
template <typename T>
std::shared_ptr<T>
staticPtr(T &o) {
    return std::shared_ptr<T>(std::shared_ptr<void>{}, &o);
}

export
template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

export
template <typename T>
struct LinkedListNode;

export
template <typename T>
using linked_list_ptr_t = const LinkedListNode<T> *;

export
template <typename T>
struct LinkedListNode {
    template <typename... Args>
    explicit
    LinkedListNode(linked_list_ptr_t<T> next, Args &&...args)
        : next(next), value(std::forward<Args>(args)...) {}

    linked_list_ptr_t<T> next;
    T value;
};

template <typename T>
struct LinkedListIterator {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = const T;

    linked_list_ptr_t<T> list;

    value_type &operator *() const { return list->value; }
    LinkedListIterator &operator ++() { list = list->next; return *this; }
    LinkedListIterator operator ++(int) { auto copy = *this; ++*this; return copy; }

    friend
    bool
    operator ==(
        const LinkedListIterator &left,
        const LinkedListIterator &right
    ) = default;
};

static_assert(std::forward_iterator<LinkedListIterator<int>>);

export
template <typename T>
LinkedListIterator<T>
begin(linked_list_ptr_t<T> list) {
    return LinkedListIterator<T>{list};
}

export
template <typename T>
LinkedListIterator<T>
end(linked_list_ptr_t<T>) {
    return LinkedListIterator<T>{nullptr};
}

export
class Cisref { // Case-insensitive string reference (non-owning)
public:
    Cisref() = default;

    // Do not accept string_view to prevent initialization from an std::string.
    // We don't want to be accidentally initialized with a pointer into a short-lived object.
    explicit constexpr Cisref(const char *p, std::size_t size) : view_(p, size) {}

    std::string_view view() const { return view_; }

    std::string str() const { return std::string(view_); }

private:
    std::string_view view_;

    friend bool
    operator==(const Cisref &left, const Cisref &right) {
        auto lowerer = [](char c) { return std::tolower(c); };

        return std::ranges::equal(
            left.view_ | std::views::transform(lowerer),
            right.view_ | std::views::transform(lowerer)
        );
    }
};

template <>
struct std::formatter<Cisref> : std::formatter<std::string_view> {
    auto format(const Cisref &ref, auto& ctx) const {
        return std::formatter<std::string_view>::format(ref.view(), ctx);
    }
};

template <>
struct std::hash<Cisref> {
    std::size_t operator()(const Cisref &ref) const {
        // 64-bit FNV-1a
        std::uint64_t h = 0xcbf2'9ce4'8422'2325;
        for (auto c : ref.view()) {
            h ^= std::uint64_t(std::tolower(c));
            h *= 0x100'0000'01b3;
        }
        return std::size_t(h);
    }
};

export
constexpr Cisref
operator ""_ci(const char *p, std::size_t size) {
    return Cisref(p, size);
}
