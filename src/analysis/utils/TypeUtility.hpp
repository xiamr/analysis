//
// Created by xiamr on 7/17/19.
//

#ifndef TINKER_TYPEUTILITY_HPP
#define TINKER_TYPEUTILITY_HPP

#include <boost/any.hpp>
#include <boost/type_index.hpp>
#include <typeindex>

#include "VectorSelector.hpp"
#include "ana_module/AbstractAnalysis.hpp"
#include "common.hpp"
#include "dsl/AmberMask.hpp"
#include "utils/IntertiaVector.hpp"

template <typename... Ts>
class TypeIs;

template <>
class TypeIs<> {
public:
    bool operator()(const boost::any &) { return false; }

    std::vector<std::string> pretty_names() { return {}; }
};

template <typename T>
inline std::string pretty_name() {
    return boost::typeindex::type_id<T>().pretty_name();
}

template <>
inline std::string pretty_name<std::string>() {
    return "string";
}

template <>
inline std::string pretty_name<Grid>() {
    return "Grid";
}

template <>
inline std::string pretty_name<AmberMask>() {
    return "AmberMask";
}

template <>
inline std::string pretty_name<std::shared_ptr<VectorSelector>>() {
    return "VectorSelector";
}

template <>
inline std::string pretty_name<std::shared_ptr<AbstractAnalysis>>() {
    return "BasicAnalysis";
}

template <typename T, typename... Ts>
class TypeIs<T, Ts...> {
public:
    bool operator()(const boost::any &val) { return val.type() == typeid(T) or TypeIs<Ts...>()(val); }

    std::vector<std::string> pretty_names() {
        auto v = TypeIs<Ts...>().pretty_names();
        v.insert(v.begin(), pretty_name<T>());
        return v;
    }
};

template <typename... Ts>
class TypePrettyNames {
public:
    std::vector<std::string> operator()() { return TypeIs<Ts...>().pretty_names(); }
};

std::string getPrettyName(const boost::any &v);

inline double cast_to_double(const boost::any &v) {
    if (v.type() == typeid(int)) {
        return boost::any_cast<int>(v);
    } else if (v.type() == typeid(double)) {
        return boost::any_cast<double>(v);
    } else if (v.type() == typeid(bool)) {
        return boost::any_cast<bool>(v);
    } else {
        throw std::runtime_error("convet to double failed!");
    }
}

class AutoConvert {
    const boost::any &v;

public:
    AutoConvert(const boost::any &v) : v(v) {}

    operator int() const { return boost::any_cast<int>(v); }

    operator double() const { return cast_to_double(v); }

    operator bool() const { return boost::any_cast<bool>(v); }

    operator std::string() const { return boost::any_cast<std::string>(v); }

    operator AmberMask() const { return boost::any_cast<AmberMask>(v); }

    operator Grid() const { return boost::any_cast<Grid>(v); }

    operator std::shared_ptr<VectorSelector>() const { return boost::any_cast<std::shared_ptr<VectorSelector>>(v); }

    operator std::shared_ptr<AbstractAnalysis>() const { return boost::any_cast<std::shared_ptr<AbstractAnalysis>>(v); }

    operator IntertiaVector() const { return boost::any_cast<IntertiaVector>(v); }
};

#endif  // TINKER_TYPEUTILITY_HPP
