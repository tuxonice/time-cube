#ifndef WEBUI_H
#define WEBUI_H

#include <Arduino.h>
#include <WebServer.h>

extern WebServer webServer;

void setupWebServer();

void handle_OnConnect();
void handle_NotFound();
void handle_Update();

void sendPage(const String& alertMessage);
String getAlertMessageHtml(const String& type, const String& message);

#endif
