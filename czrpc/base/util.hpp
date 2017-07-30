#pragma once

#include <string.h>
#include <string>
#include <vector>

namespace czrpc
{
namespace base
{
struct endpoint
{
    std::string ip;
    std::string port;
};

inline std::vector<std::string> split(const std::string& str, const std::string& delimiter)
{
    char* save = nullptr;
#ifdef _WIN32
    char* token = strtok_s(const_cast<char*>(str.c_str()), delimiter.c_str(), &save);
#else
    char* token = strtok_r(const_cast<char*>(str.c_str()), delimiter.c_str(), &save);
#endif
    std::vector<std::string> result;
    while (token != nullptr)
    {
        result.emplace_back(token);
#ifdef _WIN32
        token = strtok_s(nullptr, delimiter.c_str(), &save);
#else
        token = strtok_r(nullptr, delimiter.c_str(), &save);
#endif
    }
    return result;
}

inline bool contains(const std::string& str, const std::string& token)
{
    return str.find(token) == std::string::npos ? false : true;
}

inline endpoint get_endpoint(const std::string& remote_address)
{
    if (!contains(remote_address, ":"))
    {
        throw std::runtime_error("Remote address format error");
    }

    std::string address = remote_address;
    auto vec = split(remote_address, ":");
    if (vec.size() != 2)
    {
        throw std::runtime_error("Remote address format error");
    }

    return std::move(endpoint{ vec[0], vec[1] });
}

}
}

