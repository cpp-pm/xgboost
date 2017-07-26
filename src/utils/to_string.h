/*!
 * Copyright 2017 by Contributors
 * \file to_string.h
 * \brief Add missing std::to_string.  Warning: This adds std::to_string() to support
 * cereal serialization w/ toolchains where the C++11 implementaiton is incomplete 
 * (i.e., ANDROID libstdc++)
 * This header should be kept private.
 */

#if defined(XGBOOST_ADD_TO_STRING)

#ifndef to_string_h
#define to_string_h

#include <string>
#include <sstream>
#include <cstdlib>

namespace std {

template <typename T>
inline std::string to_string(T value)
{
    std::ostringstream os;
    os << value;
    return os.str();
}

template <typename T>
inline T stringTo(const std::string& s)
{
    std::stringstream conv;
    conv << s;
    T t;
    conv >> t;
    return t;
}

inline long double strtold(const std::string& s)
{
    return stringTo<long double>(s);
}
inline long double strtold(const char* str, char** str_end)
{
    return strtod(str, str_end);
}
inline long long stoll(const std::string& s)
{
    return stringTo<long long>(s);
}
inline int stoi(const std::string& s)
{
    return stringTo<int>(s);
}
inline unsigned long stoul(const std::string& s)
{
    return stringTo<unsigned long>(s);
}
inline unsigned long long stoull(const std::string& s)
{
    return stringTo<unsigned long long>(s);
}
inline float stof(const std::string& s)
{
    return stringTo<float>(s);
}
inline long stol(const std::string& s)
{
    return stringTo<long>(s);
}
inline double stod(const std::string& s)
{
    return stringTo<double>(s);
}
inline long double stold(const std::string& s)
{
    return stringTo<long double>(s);
}

} // namespace std

#endif // to_string_h

#endif // XGBOOST_ADD_TO_STRING
