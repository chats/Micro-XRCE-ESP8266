#include "SensorData.h"

#include <ucdr/microcdr.h>
#include <string.h>

bool SensorData_serialize_topic(ucdrBuffer* writer, const SensorData* topic)
{
    bool success = true;
    success &= ucdr_serialize_string(writer, topic->device_id);
    success &= ucdr_serialize_double(writer, topic->temperature);
    success &= ucdr_serialize_double(writer, topic->humidity);
    success &= ucdr_serialize_int32_t(writer, topic->timestamp);

    return success && !writer->error;

}

bool SensorData_deserialize_topic(ucdrBuffer* reader, SensorData* topic)
{
    bool success = true;

    success &= ucdr_deserialize_string(reader, topic->device_id, 32);
    success &= ucdr_deserialize_double(reader, &topic->temperature);
    success &= ucdr_deserialize_double(reader, &topic->humidity);
    success &= ucdr_deserialize_int32_t(reader, &topic->timestamp);

    return success && !reader->error;
}

uint32_t SensorData_size_of_topic(const SensorData* topic, uint32_t size)
{
    if (topic == NULL) {
        return 0;
    }

    uint32_t previousSize = size;
    size += ucdr_alignment(size, 4) + 4 + (uint32_t)strlen(topic->device_id) + 1;
    size += ucdr_alignment(size, 8) + 8;
    size += ucdr_alignment(size, 8) + 8;
    size += ucdr_alignment(size, 4) + 4;

    return size - previousSize;


    return size - previousSize;


}