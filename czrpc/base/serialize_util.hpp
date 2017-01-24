#pragma once

#include <memory>
#include <google/protobuf/message.h>
#include "singleton.hpp"

namespace czrpc
{
namespace base
{

class serialize_util
{
    DEFINE_SINGLETON(serialize_util);
public:
    serialize_util() = default;

    std::string serialize(std::shared_ptr<google::protobuf::Message>& message)
    {
        return message->SerializeAsString();
    }

    std::shared_ptr<google::protobuf::Message> deserialize(const std::string& message_name, const std::string& body)
    {
        std::shared_ptr<google::protobuf::Message> message = create_message(message_name);
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
    std::shared_ptr<google::protobuf::Message> create_message(const std::string& message_name)
    {
        const auto descriptor = google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(message_name);
        if (descriptor)
        {
            const auto prototype = google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
            if (prototype)
            {
                return std::shared_ptr<google::protobuf::Message>(prototype->New());            
            }
        }
        return nullptr;
    }
};

}
}
