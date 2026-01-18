#ifndef WEB_CONFIG_PORTAL_H
#define WEB_CONFIG_PORTAL_H

#include <WebServer.h> // Include necessary headers for declarations

// Declare the server object as extern
extern WebServer server;

// Declare constants as extern
extern const char* ap_ssid;
extern const char* ap_password;
extern const char* htmlContent;

// Function prototypes
bool saveConfig(const String& ssid, const String& password, const String& device_id, int num_leds);
void handleRoot();
void handleSave();
void handleNotFound();
void startConfigPortal();

#endif // WEB_CONFIG_PORTAL_H