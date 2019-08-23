#pragma once

#include "config.hpp"

namespace modmqttd {

/**
 * Simple class for encapsulating data sent between modbus and main thread
 * Thread-safe if you use classes without references and pointers
 * */
class QueueItem {
    public:
        QueueItem() {
            mTypeHash = 0;
            mItem = NULL;
        }
        /**
         * Prepare data for inserting into queue
         * */
        template<typename T> static QueueItem create(const T& data) {
            return QueueItem(typeid(T).hash_code(), new T(data));
        }

        /**
         * Get data from queue item. Once called QueueItem releases its data.
         * */
        template<typename T> std::unique_ptr<T> getData() {
            if (mItem == NULL)
                throw ModMqttProgramException("Tried to get data from queue item twice");
            const std::type_info& destType(typeid(T));
            if (!isSameAs(destType))
                throw ModMqttProgramException(std::string("Trying to get ") + destType.name() + " from wrong item");

            std::unique_ptr<T> ret((T*)mItem);
            mItem = NULL;
            return ret;
        }
        bool isSameAs(const std::type_info& type) const {
            return type.hash_code() == mTypeHash;
        }

    private:
        QueueItem(size_t typeHash,  void* item) : mTypeHash(typeHash), mItem(item) {}
        size_t mTypeHash;
        void* mItem;
};

}
