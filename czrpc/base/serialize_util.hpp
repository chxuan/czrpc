#pragma once

#include <memory>
#include <google/protobuf/message.h>
#include "singleton.hpp"

using message_ptr = std::shared_ptr<google::protobuf::Message>;

namespace czrpc
{
namespace base
{

class serialize_util
{
    DEFINE_SINGLETON(serialize_util);
public:
    serialize_util() = default;

    std::string serialize(const message_ptr& message)
    {
        return message->SerializeAsString();
    }

    message_ptr deserialize(const std::string& message_name, const std::string& body)
    {
        auto message = create_message(message_name);
        if (message == nullptr)
        {
            throw std::runtime_error("Message is nullptr");
        }
        if (!message->ParseFromString(body))
        {
            throw std::runtime_error("Parse from string failed");
        }
        return message;
    }

private:
    message_ptr create_message(const std::string& message_name)
    {
        const auto descriptor = google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(message_name);
        if (descriptor)
        {
            const auto prototype = google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
            if (prototype)
            {
                return message_ptr(prototype->New());            
            }
        }
        return nullptr;
    }
};

}
}
