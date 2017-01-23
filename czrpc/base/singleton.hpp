#pragma once

#include <iostream>
#include <mutex>

namespace czrpc
{
namespace base
{
template<typename T>
class singleton_helper
{
public:
    singleton_helper() = delete;
    virtual ~singleton_helper() = delete;
    singleton_helper(const singleton_helper&) = delete;
    singleton_helper& operator=(const singleton_helper&) = delete;

    static T* get()
    {
        static T t;
        return &t;
    }
};

#define DEFINE_SINGLETON(class_name) \
public: \
friend class singleton_helper<class_name>; \
using singleton = singleton_helper<class_name>; \
private: \
virtual ~class_name() {} \
class_name(const class_name&) = delete; \
class_name& operator=(const class_name&) = delete; \
public: 

template<typename T>
class singleton_helper_with_param
{
public:
    singleton_helper_with_param() = delete;
    virtual ~singleton_helper_with_param() = delete;
    singleton_helper_with_param(const singleton_helper_with_param&) = delete;
    singleton_helper_with_param& operator=(const singleton_helper_with_param&) = delete;

    template<typename... Args>
    static void create(Args&&... args)
    {
        if (t_ == nullptr)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (t_ == nullptr)
            {
                t_ = new T(std::forward<Args>(args)...);
            }
        }
    }

    static void destroy()
    {
        if (t_ != nullptr)
        {
            delete t_;
            t_ = nullptr;
        }
    }

    static T* get()
    {
        return t_;
    }

private:
    static T* t_;
    static std::mutex mutex_;
};
template<typename T> T* singleton_helper_with_param<T>::t_ = nullptr;
template<typename T> std::mutex singleton_helper_with_param<T>::mutex_;

#define DEFINE_SINGLETON_WITH_PARAM(class_name) \
public: \
friend class singleton_helper_with_param<class_name>; \
using singleton = singleton_helper_with_param<class_name>; \
private: \
virtual ~class_name() {} \
class_name(const class_name&) = delete; \
class_name& operator=(const class_name&) = delete; \
public: 

}
}

