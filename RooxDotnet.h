#ifndef RooxDotnet_h
#define RooxDotnet_h

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <PZEM004Tv30.h>

struct DeviceConfig {
  const char* ssid;
  const char* password;
  const char* deviceID;
  const char* deviceName;
  const char* token;
};

struct NTCConfig {
  const int* ntcPins = nullptr;
  int ntcCount = 0;
  bool useNTC = false;
};

struct RelayConfig {
  const int* relayPins = nullptr;
  int relayCount = 0;
  bool useRelay = false;
};

struct DHTConfig {
  int dhtPin = -1;
  bool useDHT = false;
};

struct PZEMConfig {
  bool usePZEM = false;
};

class DeviceManager {
  public:
    DeviceManager(const DeviceConfig& deviceCfg, const NTCConfig& ntcCfg, const RelayConfig& relayCfg, const DHTConfig& dhtCfg, const PZEMConfig& pzemCfg);
    void begin();
    void readSensors();
    void sendData();
  private:
    // WiFi & server
    const char* _ssid;
    const char* _password;
    static constexpr const char* _server = "http://192.168.1.5:3000/sensor";
    const char* _deviceID;
    const char* _deviceName;
    const char* _token;
    bool _useNTC;
    bool _useRelay;
    bool _usePZEM;

    // DHT11
    bool _useDHT;
    int _dhtPin;
    DHT* _dht;
    float _dhtTemp, _dhtHum;

    // NTC10K
    const int* _ntcPins;
    int _ntcCount;
    float* _ntcTemps;

    // Relay
    const int* _relayPins;
    int _relayCount;

    // PZEM
    PZEM004Tv30* _pzem;
    float _voltage, _current, _power, _energy, _frequency, _pf;

    void readDHT();
    void readNTC();
    void readPZEM();
    void handleRelay(const String& payload);
    String buildJsonPayload();
    bool parseRelayResponse(const String& payload);
};

#endif 