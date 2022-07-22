module;

#include <memory>

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
