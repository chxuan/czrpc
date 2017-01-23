#ifndef _COMMON_UTIL_H
#define _COMMON_UTIL_H

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace easyrpc
{

std::string gen_uuid()
{
    boost::uuids::uuid id = boost::uuids::random_generator()();
    return boost::uuids::to_string(id);
}

}

#endif
