#pragma once

int mqtt_setup(void);
void mqtt_task(void*);
void mqtt_publish(char* topic, char* data);
void mqtt_publish_int(char* topic, int);
void mqtt_publish_float(char* topic, float);
