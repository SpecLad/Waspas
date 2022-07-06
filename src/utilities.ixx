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
