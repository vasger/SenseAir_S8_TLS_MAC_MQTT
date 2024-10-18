/*
  MIT License.
  Install MQTT lib to ESP8266.
  Tested on NodeMCU 1.0 (ESP12E)
*/

#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <PubSubClient.h>


const char* mqtt_server = "your_mqtt_servername"; // MQTT broker URL
const char* mqtt_username = "mqtt_username";      // MQTT client user name, if needed
const char* mqtt_password = "mqtt_password";      // MQTT client password, if needed
// Port MQTT SSL 8883

const char* ssid = "Your_WifiSSID"; // WiFi network SSID
const char* password = "Your_WifiPassword"; // WiFi network password



const byte s8_co2[8] = {0xfe, 0x04, 0x00, 0x03, 0x00, 0x01, 0xd5, 0xc5};
const byte s8_fwver[8] = {0xfe, 0x04, 0x00, 0x1c, 0x00, 0x01, 0xe4, 0x03};
const byte s8_id_hi[8] = {0xfe, 0x04, 0x00, 0x1d, 0x00, 0x01, 0xb5, 0xc3};
const byte s8_id_lo[8] = {0xfe, 0x04, 0x00, 0x1e, 0x00, 0x01, 0x45, 0xc3};

SoftwareSerial swSer(13, 15, false); // RX, TX

byte buf[10];



//float co2 = 1;
char co2String[6];
char UptimeString[6]; 

const String C02_STATUS_TOPIC = "sensors/"+WiFi.macAddress()+"/test/C02";
const String Uptime_STATUS_TOPIC = "sensors/"+WiFi.macAddress()+"/test/Uptime";

WiFiClientSecure espClient;         
PubSubClient client(espClient);


//void MQTT_connect();

void myread(int n) {
  int i;
  memset(buf, 0, sizeof(buf));
  for(i = 0; i < n; ) {
    if(swSer.available() > 0) {
      buf[i] = swSer.read();
      i++;
    }
    yield();
    delay(10);
  }
}

// Compute the MODBUS RTU CRC
uint16_t ModRTU_CRC(byte* buf, int len){
  uint16_t crc = 0xFFFF;
  
  for (int pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buf[pos];          // XOR byte into least sig. byte of crc
  
    for (int i = 8; i != 0; i--) {    // Loop over each bit
      if ((crc & 0x0001) != 0) {      // If the LSB is set
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      }
      else                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
    }
  }
  // Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)
  return crc;  
}

void setup() {
  Serial.begin(115200);
  swSer.begin(9600);
  delay(10);

  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Sensor ID: ");
  swSer.write(s8_id_hi, 8);
  myread(7);
  Serial.printf("%02x%02x", buf[3], buf[4]);
  swSer.write(s8_id_lo, 8);
  myread(7);
  Serial.printf("%02x%02x", buf[3], buf[4]);
  Serial.println("");

  swSer.write(s8_fwver, 8);
  myread(7);
  Serial.printf("Firmware: %d.%d", buf[3], buf[4]);
  Serial.println();
}

int value = 0;

void loop() {
  uint16_t co2;

  // MQTT_connect();
  co2 = readco2();
  client.setServer(mqtt_server, 8883);
  espClient.setInsecure();

  connect_mqtt(co2);
  //senseair.publish(co2);
  Serial.println("Closing connection to MQTT broker...");
  client.disconnect();

  
  delay(30 * 1000L);
}

uint16_t readco2() {
  uint16_t crc, got, co2;
  
  swSer.write(s8_co2, 8);
  myread(7);
  co2 = (uint16_t)buf[3] * 256 + (uint16_t)buf[4];
  crc = ModRTU_CRC(buf, 5);
  got = (uint16_t)buf[5] + (uint16_t)buf[6] * 256;
  if(crc != got) {
    Serial.print("Invalid checksum.  Expect: ");
    Serial.print(crc, HEX);
    Serial.print("  Got: ");
    Serial.println(got, HEX);
  } else {
    Serial.print("CO2: ");
    Serial.println(co2);
    Serial.printf("%02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);
  }
  return co2;
}

void connect_mqtt(uint16_t co2) //Connecting to the MQTT broker
 {
   while (!client.connected())
    {
    Serial.print("Connecting to MQTT broker at ");
    Serial.print(mqtt_server);
    Serial.print("...");

    // Attempt to connect
    if (client.connect(("ESP8266Client-"+WiFi.macAddress()).c_str(), mqtt_username, mqtt_password))
      {
        Serial.println(" connected!");
  
        // Once connected, publish an announcement...
        Serial.println("Sending topic(s)...");
        client.publish(C02_STATUS_TOPIC.c_str(), dtostrf(co2, 2, 2, co2String ));
        client.publish(Uptime_STATUS_TOPIC.c_str(), dtostrf(millis(), 2, 2, UptimeString));        
        Serial.println("Sending topic(s)... done");
      }
    else
      {
        Serial.print("Connection failed, rc=");
        Serial.print(client.state());
        Serial.println(" will retry in 5 seconds");
  
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
}