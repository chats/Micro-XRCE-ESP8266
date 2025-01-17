#include <Arduino.h>
#include "SensorData.h"
#include <uxr/client/client.h>
#include <ucdr/microcdr.h>
#include <spi_flash.h>
#include <stdio.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#include "DHT.h"

// Constants
#define STREAM_HISTORY 2
#define BUFFER_SIZE UXR_CONFIG_UDP_TRANSPORT_MTU * STREAM_HISTORY
//#define SSID "Chats-WIFI"
//#define PASSWD "0899247328"
#define SSID "AEC-WiFi"
#define PASSWD "AEC261058"
//#define AGENT_IP "192.168.254.126"
#define AGENT_IP "192.168.1.9"
#define AGENT_PORT 2018
#define MAX_TOPICS 1000
#define MAX_AGENTS 10
#define DHTPIN D1
//#define DHTTYPE DHT11
#define DHTTYPE DHT22

// Global variables
DHT dht(DHTPIN, DHTTYPE);
uxrAgentAddress agent;

// Function declarations
void connectToWifi();
void setupTime();
bool readSensorData(float& temperature, float& humidity);
bool createSession(uxrUDPTransport& transport, uxrSession& session, uxrUDPPlatform& udp_platform, const char* ip_copy);
bool createEntities(uxrSession& session, uxrStreamId reliable_out);

void on_agent_found(const uxrAgentAddress* address, void* args) {
   (void)args;
   agent.ip = address->ip;
   agent.port = address->port;
   printf("Found agent => ip: %s, port: %d\n", agent.ip, agent.port);
}

void connectToWifi() {
   WiFi.mode(WIFI_STA);
   WiFi.begin(SSID, PASSWD);

   int attempts = 0;
   const int maxAttempts = 20;

   while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
       delay(500);
       Serial.print(".");
       attempts++;
   }

   if (WiFi.status() != WL_CONNECTED) {
       Serial.println("\nFailed to connect. Restarting...");
       ESP.restart();
   }

   printf("\n Connected to: %s \n", SSID);
   printf("IP Address: %s \n", WiFi.localIP().toString().c_str());
}

void setupTime() {
   configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
   Serial.println("Waiting for time sync");

   while (time(nullptr) < 1600000000) {
       Serial.print(".");
       delay(100);
   }
   Serial.println("\nTime synchronized");
}

bool readSensorData(float& temperature, float& humidity) {
   // รอให้เซ็นเซอร์พร้อม
   delay(2000);  // DHT11 ต้องการเวลาอย่างน้อย 2 วินาที

   // อ่านค่า
   humidity = dht.readHumidity();
   temperature = dht.readTemperature();

   // ตรวจสอบค่า NaN
   if (isnan(humidity) || isnan(temperature)) {
       Serial.println(F("Failed to read from DHT sensor!"));
       return false;
   }

   // ตรวจสอบช่วงค่าที่เป็นไปได้
   // DHT11: Humidity 20-90%, Temperature 0-50°C
   if (humidity < 0 || humidity > 100 || temperature < -10 || temperature > 85) {
       Serial.println(F("Sensor readings out of range!"));
       return false;
   }

   // แสดงค่าที่อ่านได้
   //Serial.print(F("Temperature: "));
   //Serial.print(temperature);
   //Serial.print(F("°C, Humidity: "));
   //Serial.print(humidity);
   //Serial.println(F("%"));

   return true;
}

bool createSession(uxrUDPTransport& transport, uxrSession& session, uxrUDPPlatform& udp_platform, const char* ip_copy) {
   if (!uxr_init_udp_transport(&transport, &udp_platform, ip_copy, agent.port)) {
       Serial.print("Error at create transport.\n");
       return false;
   }
   Serial.print("Created transport.\n");

   uxr_init_session(&session, &transport.comm, 0xAAAABBBB);
   if (!uxr_create_session(&session)) {
       printf("Error at create session.\n");
       return false;
   }
   printf("Created Session.\n");
   return true;
}

bool createEntities(uxrSession& session, uxrStreamId reliable_out) {
   const char* participant_xml = "<dds><participant><rtps><name>esp8266_xrce_participant</name></rtps></participant></dds>";
   const char* topic_xml = "<dds><topic><name>SensorData</name><dataType>SensorData</dataType></topic></dds>";
   const char* publisher_xml = "";
   const char* datawriter_xml = "<dds><data_writer><topic><kind>NO_KEY</kind><name>SensorData</name><dataType>SensorData</dataType></topic></data_writer></dds>";

   uxrObjectId participant_id = uxr_object_id(0x01, UXR_PARTICIPANT_ID);
   uxrObjectId topic_id = uxr_object_id(0x01, UXR_TOPIC_ID);
   uxrObjectId publisher_id = uxr_object_id(0x01, UXR_PUBLISHER_ID);
   uxrObjectId datawriter_id = uxr_object_id(0x01, UXR_DATAWRITER_ID);

   uint16_t participant_req = uxr_buffer_create_participant_xml(&session, reliable_out, participant_id, 0, participant_xml, UXR_REPLACE);
   uint16_t topic_req = uxr_buffer_create_topic_xml(&session, reliable_out, topic_id, participant_id, topic_xml, UXR_REPLACE);
   uint16_t publisher_req = uxr_buffer_create_publisher_xml(&session, reliable_out, publisher_id, participant_id, publisher_xml, UXR_REPLACE);
   uint16_t datawriter_req = uxr_buffer_create_datawriter_xml(&session, reliable_out, datawriter_id, publisher_id, datawriter_xml, UXR_REPLACE);

   uint8_t status[4];
   uint16_t requests[4] = {participant_req, topic_req, publisher_req, datawriter_req};

   if (!uxr_run_session_until_all_status(&session, 1000, requests, status, 4)) {
       printf("Error at create entities: participant: %i topic: %i publisher: %i datawriter: %i\n", 
              status[0], status[1], status[2], status[3]);
       return false;
   }
   printf("Created Entities.\n");
   return true;
}

void setup() {
   Serial.begin(9600);
   dht.begin();

   WiFiUDP udp_instance;

   //agent.ip = "192.168.254.126";
   //agent.port = 2018;
   agent.ip = AGENT_IP;
   agent.port = AGENT_PORT;

   connectToWifi();
   setupTime();

   uxr_discovery_agents_default(2, 1000, on_agent_found, NULL, &udp_instance);

   uint32_t count = 0;
   const char* ip_copy = strdup(AGENT_IP);

   while (count != MAX_TOPICS) {
       uxrUDPTransport transport;
       uxrUDPPlatform udp_platform;
       udp_platform.udp_instance = &udp_instance;
       uxrSession session;

       uint8_t output_reliable_stream_buffer[BUFFER_SIZE];
       uint8_t input_reliable_stream_buffer[BUFFER_SIZE];

       if (!createSession(transport, session, udp_platform, ip_copy)) {
           continue;
       }

       uxrStreamId reliable_out = uxr_create_output_reliable_stream(&session, output_reliable_stream_buffer, BUFFER_SIZE, STREAM_HISTORY);
       uxr_create_input_reliable_stream(&session, input_reliable_stream_buffer, BUFFER_SIZE, STREAM_HISTORY);

       if (!createEntities(session, reliable_out)) {
           continue;
       }

       float temperature, humidity;
       if (!readSensorData(temperature, humidity)) {
           delay(2000);
           continue;
       }

       int32_t ts = (int32_t)time(nullptr);
       bool connected = true;

       while (connected && count < MAX_TOPICS) {
           // อ่านค่าใหม่ทุกครั้งที่จะส่ง
           if (!readSensorData(temperature, humidity)) {
               delay(2000);
               continue;
           }

           //SensorData topic = {"esp8266", temperature, humidity, ts};
           SensorData topic;
           strcpy(topic.device_id, "esp8266");
           topic.temperature = temperature;
           topic.humidity = humidity;
           topic.timestamp = ts;

           ucdrBuffer ub;
           uint32_t topic_size = SensorData_size_of_topic(&topic, 0);

           if (topic_size > BUFFER_SIZE) {
               Serial.println("Error: Topic size exceeds buffer size");
               continue;
           }

           uxr_prepare_output_stream(&session, reliable_out, uxr_object_id(0x01, UXR_DATAWRITER_ID), &ub, topic_size);
           SensorData_serialize_topic(&ub, &topic);

           printf("Send topic: device_id=%s, temperature=%.1f, humidity=%.1f, timestamp=%d\n", 
                  topic.device_id, topic.temperature, topic.humidity, topic.timestamp);

           connected = uxr_run_session_until_confirm_delivery(&session, 1000);
           delay(2000);
           count++;
       }

       if (!(count == MAX_TOPICS)) {
           if (!(WiFi.status() == WL_CONNECTED)) {
               printf("%s not found... Reconnecting\n", SSID);
               connectToWifi();
           } else if (!connected) {
               printf("Master Missing!!!\nReconnecting...\n");
           }
       }
   }

   if (ip_copy != NULL) {
       free((void*)ip_copy);
   }
}

void loop() {
   // Empty as all work is done in setup()
}