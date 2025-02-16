//Since we are using a small private wifi we might get away with this
//Usually this file should be git-ignored to not reveal any secrets
#ifndef SECRETS_H
#define SECRETS_H

extern const char* ssid;
extern const char* password;
extern const char* mqtt_topic;
extern const char* mqtt_pass;
extern const char* mqtt_user;
extern const char* mqtt_port;
extern const char* mqtt_server;
extern const char* postserver;
extern const char* postresource;

#endif // SECRETS_H