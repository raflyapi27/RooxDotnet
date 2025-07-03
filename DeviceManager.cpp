#include "RooxDotnet.h"

DeviceManager::DeviceManager(const DeviceConfig& deviceCfg, const NTCConfig& ntcCfg, const RelayConfig& relayCfg, const DHTConfig& dhtCfg, const PZEMConfig& pzemCfg)
  : _ssid(deviceCfg.ssid), _password(deviceCfg.password),
    _deviceID(deviceCfg.deviceID), _deviceName(deviceCfg.deviceName), _token(deviceCfg.token),
    _useNTC(ntcCfg.useNTC), _useRelay(relayCfg.useRelay), _usePZEM(pzemCfg.usePZEM),
    _ntcPins(ntcCfg.ntcPins), _ntcCount(ntcCfg.ntcCount),
    _relayPins(relayCfg.relayPins), _relayCount(relayCfg.relayCount),
    _dhtPin(dhtCfg.dhtPin), _useDHT(dhtCfg.useDHT)
{
  _dht = _useDHT ? new DHT(_dhtPin, DHT11) : nullptr;
  _ntcTemps = _useNTC ? new float[_ntcCount] : nullptr;
  _pzem = _usePZEM ? new PZEM004Tv30(Serial2, 16, 17) : nullptr;
}

void DeviceManager::begin() {
  Serial.begin(115200);
  if (_useNTC || _usePZEM) analogReadResolution(12);
  if (_useRelay) {
    for (int i = 0; i < _relayCount; i++) {
      pinMode(_relayPins[i], OUTPUT);
      digitalWrite(_relayPins[i], LOW);
    }
  }
  if (_usePZEM) Serial2.begin(9600, SERIAL_8N1, 16, 17);
  if (_useDHT) _dht->begin();

  WiFi.begin(_ssid, _password);
  Serial.print("Menghubungkan ke WiFi");
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nTerhubung ke WiFi! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nGagal koneksi WiFi.");
  }
}

void DeviceManager::readSensors() {
  if (_useDHT) readDHT();
  if (_useNTC) readNTC();
  if (_usePZEM) readPZEM();

  Serial.println("=== Data Sensor ===");
  if (_useDHT) Serial.printf("Suhu: %.2f °C, Kelembaban: %.2f%%\n", _dhtTemp, _dhtHum);
  if (_useNTC) for (int i = 0; i < _ntcCount; i++) Serial.printf("NTC10K %d: %.2f °C\n", i + 1, _ntcTemps[i]);
  if (_usePZEM) {
    Serial.printf("Tegangan: %.2f V\n", _voltage);
    Serial.printf("Arus: %.2f A\n", _current);
    Serial.printf("Daya: %.2f W\n", _power);
    Serial.printf("Energi: %.3f kWh\n", _energy);
    Serial.printf("Frekuensi: %.1f Hz\n", _frequency);
    Serial.printf("Power Factor: %.2f\n", _pf);
  }
  Serial.println("====================\n");
}

void DeviceManager::readDHT() {
  _dhtTemp = _dht->readTemperature();
  _dhtHum = _dht->readHumidity();
  if (isnan(_dhtTemp) || isnan(_dhtHum)) {
    Serial.println("Gagal membaca data dari sensor DHT11!");
    _dhtTemp = _dhtHum = 0;
  }
}

void DeviceManager::readNTC() {
  for (int i = 0; i < _ntcCount; i++) {
    int analogValue = analogRead(_ntcPins[i]);
    float resistance = 10000.0 * ((4095.0 / analogValue) - 1.0);
    float steinhart = resistance / 10000.0;
    steinhart = log(steinhart);
    steinhart /= 3950.0;
    steinhart += 1.0 / (25.0 + 273.15);
    steinhart = 1.0 / steinhart;
    steinhart -= 273.15;
    _ntcTemps[i] = roundf(steinhart * 100) / 100.0;
  }
}

void DeviceManager::readPZEM() {
  _voltage = isnan(_pzem->voltage()) ? 0 : _pzem->voltage();
  _current = isnan(_pzem->current()) ? 0 : _pzem->current();
  _power = isnan(_pzem->power()) ? 0 : _pzem->power();
  _energy = isnan(_pzem->energy()) ? 0 : _pzem->energy();
  _frequency = isnan(_pzem->frequency()) ? 0 : _pzem->frequency();
  _pf = isnan(_pzem->pf()) ? 0 : _pzem->pf();

  // Validasi nilai agar tidak aneh
  if (_voltage < 0 || _voltage > 300) _voltage = 0;
  if (_current < 0 || _current > 100) _current = 0;
  if (_power < 0 || _power > 10000) _power = 0;
  if (_energy < 0 || _energy > 100000) _energy = 0;
  if (_frequency < 40 || _frequency > 70) _frequency = 0;
  if (_pf < 0 || _pf > 1.2) _pf = 0;
}

void DeviceManager::sendData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(_server);
    http.addHeader("Content-Type", "application/json");

    String json = buildJsonPayload();

    int httpResponseCode = http.POST(json);
    if (httpResponseCode > 0) {
      String payload = http.getString();
      if (_useRelay) handleRelay(payload);
    } else {
      Serial.print("Koneksi WiFi OK, tapi gagal hubungi server. Error: ");
      Serial.println(http.errorToString(httpResponseCode));
    }
    http.end();
  } else {
    Serial.println("Server tidak terhubung");
  }
}

String DeviceManager::buildJsonPayload() {
  String json = "{";
  json += "\"deviceID\":\"" + String(_deviceID) + "\",";
  json += "\"deviceName\":\"" + String(_deviceName) + "\",";
  json += "\"token\":\"" + String(_token) + "\"";
  
  if (_useDHT) {
    json += ",\"dhtTemps\":[" + String(_dhtTemp, 2) + "]";
    json += ",\"humidity\":[" + String(_dhtHum, 2) + "]";
  }
  
  if (_useNTC) {
    json += ",\"temps\":[";
    for (int i = 0; i < _ntcCount; i++) {
      if (i > 0) json += ",";
      json += String(_ntcTemps[i], 2);
    }
    json += "]";
  }
  
  if (_usePZEM) {
    json += ",\"voltage\":" + String(_voltage, 2);
    json += ",\"current\":" + String(_current, 2);
    json += ",\"power\":" + String(_power, 2);
    json += ",\"energy\":" + String(_energy, 3);
    json += ",\"frequency\":" + String(_frequency, 1);
    json += ",\"pf\":" + String(_pf, 2);
  }
  
  json += "}";
  return json;
}

void DeviceManager::handleRelay(const String& payload) {
  if (parseRelayResponse(payload)) {
    Serial.println("Relay states updated successfully");
  } else {
    Serial.println("Failed to parse relay response");
  }
}

bool DeviceManager::parseRelayResponse(const String& payload) {
  // Simple JSON parsing for relay states
  // Looking for pattern: "relayStates":{"relay1":true,"relay2":false,...}
  
  int relayStatesStart = payload.indexOf("\"relayStates\":");
  if (relayStatesStart == -1) return false;
  
  int braceStart = payload.indexOf("{", relayStatesStart);
  if (braceStart == -1) return false;
  
  int braceCount = 0;
  int braceEnd = -1;
  
  for (int i = braceStart; i < payload.length(); i++) {
    if (payload.charAt(i) == '{') braceCount++;
    if (payload.charAt(i) == '}') {
      braceCount--;
      if (braceCount == 0) {
        braceEnd = i;
        break;
      }
    }
  }
  
  if (braceEnd == -1) return false;
  
  String relayStatesJson = payload.substring(braceStart + 1, braceEnd);
  
  // Parse each relay state
  for (int i = 0; i < _relayCount; i++) {
    String relayKey = "\"relay" + String(i + 1) + "\":";
    int keyPos = relayStatesJson.indexOf(relayKey);
    if (keyPos != -1) {
      int valueStart = keyPos + relayKey.length();
      int valueEnd = relayStatesJson.indexOf(",", valueStart);
      if (valueEnd == -1) valueEnd = relayStatesJson.length();
      
      String valueStr = relayStatesJson.substring(valueStart, valueEnd);
      valueStr.trim();
      
      bool state = (valueStr == "true");
      digitalWrite(_relayPins[i], state ? HIGH : LOW);
      Serial.printf("Relay %d (Pin %d): %s\n", i + 1, _relayPins[i], state ? "ON" : "OFF");
    }
  }
  
  return true;
}