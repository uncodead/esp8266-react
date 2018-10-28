#include <NTPSettingsService.h>

NTPSettingsService::NTPSettingsService(AsyncWebServer* server, FS* fs) : SettingsService(server, fs, NTP_SETTINGS_SERVICE_PATH, NTP_SETTINGS_FILE) {
  WiFi.onEvent(std::bind(&NTPSettingsService::onStationModeDisconnected, this, std::placeholders::_1, std::placeholders::_2), WiFiEvent_t::SYSTEM_EVENT_AP_STADISCONNECTED); 
  WiFi.onEvent(std::bind(&NTPSettingsService::onStationModeGotIP, this, std::placeholders::_1, std::placeholders::_2), WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);

  NTP.onNTPSyncEvent ([this](NTPSyncEvent_t ntpEvent) {
    _ntpEvent = ntpEvent;
    _syncEventTriggered = true;
  });
}

NTPSettingsService::~NTPSettingsService() {}

void NTPSettingsService::loop() {
  // detect when we need to re-configure NTP and do it in the main loop
  if (_reconfigureNTP) {
    _reconfigureNTP = false;
    configureNTP();
  }

  // output sync event to serial
  if (_syncEventTriggered) {
    processSyncEvent(_ntpEvent);
    _syncEventTriggered = false;
  }

  // keep time synchronized in background
  now();
}

void NTPSettingsService::readFromJsonObject(JsonObject& root) {
  _server = root["server"] | NTP_SETTINGS_SERVICE_DEFAULT_SERVER;
  _interval = root["interval"];

  // validate server is specified, resorting to default
  _server.trim();
  if (!_server){
    _server = NTP_SETTINGS_SERVICE_DEFAULT_SERVER;
  }

  // make sure interval is in bounds
  if (_interval < NTP_SETTINGS_MIN_INTERVAL){
    _interval = NTP_SETTINGS_MIN_INTERVAL;
  } else if (_interval > NTP_SETTINGS_MAX_INTERVAL) {
    _interval = NTP_SETTINGS_MAX_INTERVAL;
  }
}

void NTPSettingsService::writeToJsonObject(JsonObject& root) {
  root["server"] = _server;
  root["interval"] = _interval;
}

void NTPSettingsService::onConfigUpdated() {
  _reconfigureNTP = true;
}

void NTPSettingsService::onStationModeGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("Got IP address, starting NTP Synchronization");
  _reconfigureNTP = true;
}

void NTPSettingsService::onStationModeDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("WiFi connection dropped, stopping NTP.");

  // stop NTP synchronization, ensuring no re-configuration can take place
  _reconfigureNTP = false;
  NTP.stop();
}

void NTPSettingsService::configureNTP() {
  Serial.println("Configuring NTP...");

  // disable sync
  NTP.stop();

  // enable sync
  NTP.begin(_server);
  NTP.setInterval(_interval);
}

void NTPSettingsService::processSyncEvent(NTPSyncEvent_t ntpEvent) {
    if (ntpEvent) {
        Serial.print ("Time Sync error: ");
        if (ntpEvent == noResponse)
            Serial.println ("NTP server not reachable");
        else if (ntpEvent == invalidAddress)
            Serial.println ("Invalid NTP server address");
    } else {
        Serial.print ("Got NTP time: ");
        Serial.println (NTP.getTimeDateString (NTP.getLastNTPSync ()));
    }
}
