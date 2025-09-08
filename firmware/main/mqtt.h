#pragma once

int mqtt_setup(void);
void mqtt_task(void*);
void mqtt_publish(char* topic, char* data);

