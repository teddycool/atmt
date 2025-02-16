#include "logger.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <secrets.h>
#include <thread>
#include <future>

Logger::Logger(String chipid) {
    this->chipid = chipid;
    // No initialization needed for HTTPClient
}

Logger::~Logger() {
    // No cleanup needed for HTTPClient
}


void Logger::postlog(String msg)
{
  WiFiClient wificlient; // Used for sending http to url
  HTTPClient http;

  msg.replace(" ", "%20");
  String url = "http://192.168.2.2/post_log.php?chipid=" + chipid + "&msg=" + msg;
 // String url = "http://" + String(postserver) + String(postresource) + "?chipid=" + this->chipid + "&msg="  + msg;
  Serial.println(url);  
 
  http.begin(wificlient, url);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0)
  {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
}