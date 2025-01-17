#ifndef _SensorData_H_
#define _SensorData_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>


typedef struct SensorData
{
    char device_id[32];
    double temperature;
    double humidity;
    int32_t timestamp;
} SensorData;

struct ucdrBuffer;

bool SensorData_serialize_topic(struct ucdrBuffer* writer, const SensorData* topic);
bool SensorData_deserialize_topic(struct ucdrBuffer* reader, SensorData* topic);
uint32_t SensorData_size_of_topic(const SensorData* topic, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif // _SensorData_H_