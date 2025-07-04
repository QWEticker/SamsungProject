#include "mbed.h"
#include "TCPSocket.h"
#include "MQTTClientMbedOs.h"
#include "OPT3001.h"
#include "BME280.h"
#include "MQ135.h"
#include "TroykaOLED.h"

// ==== Пороговые значения ====
float TEMP_MIN = 20.0f;
float TEMP_MAX = 50.0f;

float HUMIDITY_MIN = 40.0f;
float HUMIDITY_MAX = 80.0f;

float PRESSURE_MIN = 712.0f;   
float PRESSURE_MAX = 790.0f;   

float LUX_MIN = 10.0f;
float LUX_MAX = 100.0f;

float CO2_MIN = 30.0f;
float CO2_MAX = 100.0f;

// ==== Управление транзисторами ====
DigitalOut WindowRele(D4);
DigitalOut LightRele(D7);

// ==== Пины ====
I2C i2c(I2C_SDA, I2C_SCL);  // Общий I2C для сенсоров и OLED
TroykaOLED display(0x3C, 128, 64);  // Адрес OLED: 0x3C

WiFiInterface *wifi;

// ==== Сенсоры ====
OPT3001 sensor_opt(I2C_SDA, I2C_SCL);
BME280 sensor_bme(I2C_SDA, I2C_SCL);
MQ135 mq135(PA_0);

bool find_json_value(const char* json, const char* key, float& value) 
{
    char key_str[32];
    sprintf(key_str, "\"%s\":", key);
    const char* key_pos = strstr(json, key_str);
    if (!key_pos) return false;
    const char* val_start = key_pos + strlen(key_str);    
    while (*val_start == ' ') val_start++;    
    char* val_end;
    value = strtof(val_start, &val_end);
    if (val_end == val_start) return false;
    return true;
}

void messageArrived(MQTT::MessageData& md) 
{
    MQTT::Message &message = md.message;
    const char* topic = md.topicName.lenstring.data;
    printf("Message arrived on topic: %s\n", topic);
    
    if (strncmp(topic, "sensor/set", strlen("sensor/set")) != 0) 
    {
        printf("Incorrect topic: %s\n", topic);
        return;
    }
    char payload[256];
    memcpy(payload, message.payload, message.payloadlen);
    payload[message.payloadlen] = '\0';
    float value;

    if (find_json_value(payload, "tmin", value)) 
    {
        TEMP_MIN = value;
        printf("tmin is set: %.2f\n", TEMP_MIN);
    }

    if (find_json_value(payload, "tmax", value)) 
    {
        TEMP_MAX = value;
        printf("tmax was set: %.2f\n", TEMP_MAX);
    }

    if (find_json_value(payload, "hmin", value)) 
    {
        HUMIDITY_MIN = value;
        printf("hmin was set: %.2f\n", HUMIDITY_MIN);
    }

    if (find_json_value(payload, "hmax", value)) 
    {
        HUMIDITY_MAX = value;
        printf("hmax was set: %.2f\n", HUMIDITY_MAX);
    }

    if (find_json_value(payload, "pmin", value)) 
    {
        PRESSURE_MIN = value;
        printf("pmin was set: %.2f\n", PRESSURE_MIN);
    }

    if (find_json_value(payload, "pmax", value)) 
    {
        PRESSURE_MAX = value;
        printf("pmax was set: %.2f\n", PRESSURE_MAX);
    }

    if (find_json_value(payload, "lmin", value))
    {
        LUX_MIN = value;
        printf("lmin was set: %.2f\n", LUX_MIN);
    }

    if (find_json_value(payload, "lmax", value)) 
    {
        LUX_MAX = value;
        printf("lmax was set: %.2f\n", LUX_MAX);
    }
    if (find_json_value(payload, "cmin", value)) 
    {
        CO2_MIN = value;
        printf("cmin was set: %.2f\n", CO2_MIN);
    }
    if (find_json_value(payload, "cmax", value)) 
    {
        CO2_MAX = value;
        printf("cmax was set: %.2f\n", CO2_MAX);
    }
    if (find_json_value(payload, "window", value)) 
    {
    if (value == 1) 
    {
        WindowRele = true;
        printf("Window was opened\n");
    } else if (value == 0) {
        WindowRele = false;
        printf("Window was closed\n");
    } else 
    {
        printf("Incorrect value 'window': %.0f\n", value);
    }
}
if (find_json_value(payload, "light", value)) 
{
    if (value == 1) 
    {
        LightRele = true;
        printf("Light on\n");
    } else if (value == 0) 
    {
        LightRele = false;
        printf("Light off\n");
    } else 
    {
        printf("Incorrect value 'light': %.0f\n", value);
    }
}
    printf("Message was processed.\n");
}

const char *sec2str(nsapi_security_t sec) 
{
    switch (sec) 
    {
        case NSAPI_SECURITY_NONE: return "None";
        case NSAPI_SECURITY_WEP: return "WEP";
        case NSAPI_SECURITY_WPA: return "WPA";
        case NSAPI_SECURITY_WPA2: return "WPA2";
        case NSAPI_SECURITY_WPA_WPA2: return "WPA/WPA2";
        default: return "Unknown";
    }
}

void checkThresholdsAndControlTransistor(MQTTClient* client, float temp, float hum, float press_mmHg, float lux, float co2) 
{
    bool triggerLightRele = false;
    bool triggerWindowRele = false;

    if (temp < TEMP_MIN) 
    {
        triggerWindowRele = false;
    }
    if (temp > TEMP_MAX) 
    {
        triggerWindowRele = true;
    }

    if (hum < HUMIDITY_MIN) 
    {
        triggerWindowRele = false;
    }
    if (hum > HUMIDITY_MAX) 
    {
        triggerWindowRele = true;
    }  

    if (press_mmHg < PRESSURE_MIN) 
    {
        triggerWindowRele = false;
    }
    if (press_mmHg > PRESSURE_MAX) 
    {
        triggerWindowRele = true;
    }   
    
    if (lux < LUX_MIN) 
    {
        triggerLightRele = true;
    }
    if (lux > LUX_MAX) 
    {
        triggerLightRele = false;
    }

    if (co2 > CO2_MAX) 
    {
        triggerWindowRele = false;
    }
    if (co2 < CO2_MIN) 
    {
        triggerWindowRele = true;
    }    

    LightRele = triggerLightRele ? 1 : 0;
    WindowRele = triggerWindowRele ? 1 : 0;    

    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"temp\":%.1f,\"hum\":%.1f,\"press\":%.1f,\"lux\":%.0f,\"co2\":%.0f,"
         "\"rele\":{\"light\":%d,\"window\":%d}}", temp, hum, press_mmHg, lux, co2,
        LightRele.read(), WindowRele.read()); 

    int temp_alert = 0;
    if (temp < TEMP_MIN) temp_alert = 2;
    else if (temp > TEMP_MAX) temp_alert = 1;

    int hum_alert = 0;
    if (hum < HUMIDITY_MIN) hum_alert = 2;
    else if (hum > HUMIDITY_MAX) hum_alert = 1;

    int press_alert = 0;
    if (press_mmHg < PRESSURE_MIN) press_alert = 2;
    else if (press_mmHg > PRESSURE_MAX) press_alert = 1;

    int lux_alert = 0;
    if (lux < LUX_MIN) lux_alert = 2;
    else if (lux > LUX_MAX) lux_alert = 1;

    int co2_alert = 0;
    if (co2 < CO2_MIN) co2_alert = 2;
    else if (co2 > CO2_MAX) co2_alert = 1;
    char payl[64];
    snprintf(payl, sizeof(payl),"{\"temp_al\":%d,\"hum_al\":%d,\"press_al\":%d,\"lux_al\":%d,\"co2_al\":%d}", 
    temp_alert, hum_alert, press_alert, lux_alert, co2_alert);    

    if (client->isConnected()) 
    {
        MQTT::Message message;
        message.qos = MQTT::QOS0;
        message.payload = (void*)payload;
        message.payloadlen = strlen(payload) + 1;
        int rc = client->publish("sensor/out_data", message);
        if (rc != 0)
            printf("Public failure MQTT: %d\n", rc);
        else
        {
            printf("MQTT -> sensor/out_data: %s\n", payload);
        }            
        MQTT::Message message1;
        message1.qos = MQTT::QOS0;
        message1.payload = (void*)payl;
        message1.payloadlen = strlen(payl) + 1;
        int rc1 = client->publish("sensor/out_alert", message1);
        if (rc1 != 0)
            printf("Public failure MQTT: %d\n", rc1);
        else
            printf("MQTT -> sensor/out_alert: %s\n", payl);        
    } 
    else 
    {
        printf("MQTT client is not connected. Public was lost.\n");
    }
}

const char* getStatusStr(float value, float min, float max) 
{
        if (value < min) return "|Низ.";
        else if (value > max) return "|Выс.";
        else return "|ОК  ";
}

void update_display(float temperature, float humidity, float pressure_mmHg, float lux, float co2) 
{
    display.clearDisplay();
    display.setFont(fontRus6x8);
    display.setCoding(Encoding::UTF8);

    char buffer[40];
    // Шапка таблицы
    display.print("Датчик|Значение|Сост", 0, 0);
    display.drawLine(0, 8, 127, 8);

    // Температура
    snprintf(buffer, sizeof(buffer), "Темп  |%.2f C %s", temperature,
             getStatusStr(temperature, TEMP_MIN, TEMP_MAX));
    display.print(buffer, 0, 10);
    display.drawLine(0, 18, 127, 18);

    // Влажность
    snprintf(buffer, sizeof(buffer), "Влаж  |%.0f %%    %s", humidity,
             getStatusStr(humidity, HUMIDITY_MIN, HUMIDITY_MAX));
    display.print(buffer, 0, 20);
    display.drawLine(0, 28, 127, 28);
    // Давление
    snprintf(buffer, sizeof(buffer), "Давл  |%.1f мм%s", pressure_mmHg,
             getStatusStr(pressure_mmHg, PRESSURE_MIN, PRESSURE_MAX));
    display.print(buffer, 0, 30);
    display.drawLine(0, 38, 127, 38);

    // Освещённость    
    if(lux < 10.2f)
    {
    snprintf(buffer, sizeof(buffer), "Осв   |%.0f лк    %s", lux,
             getStatusStr(lux, LUX_MIN, LUX_MAX));
    display.print(buffer, 0, 40);
    display.drawLine(0, 48, 127, 48);
        if(lux >= 1000.2f)
        {
            snprintf(buffer, sizeof(buffer), "Осв   |%.0f лк  %s", lux,
                    getStatusStr(lux, LUX_MIN, LUX_MAX));
            display.print(buffer, 0, 40);
            display.drawLine(0, 48, 127, 48);
        }
    }
    else 
    {
        snprintf(buffer, sizeof(buffer), "Осв   |%.0f лк   %s", lux,
             getStatusStr(lux, LUX_MIN, LUX_MAX));
        display.print(buffer, 0, 40);
        display.drawLine(0, 48, 127, 48);
    }

    

    // CO2
    snprintf(buffer, sizeof(buffer), "CO2   |%.0f ppm  %s", co2,
             getStatusStr(co2, CO2_MIN, CO2_MAX));
    display.print(buffer, 0, 50);
    display.drawLine(0, 58, 127, 58);

    display.updateAll();
}

void mqtt_demo(NetworkInterface *net) 
{
    mar:
    TCPSocket socket;
    MQTTClient client(&socket);

    const char *hostname = "test.mosquitto.org";
    int port = 1883;

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 4;
    char client_id[32];
    sprintf(client_id, "sensor_client_%08x", rand());
    data.clientID.cstring = client_id;
    data.username.cstring = NULL;
    data.password.cstring = NULL;

    while (true) 
    {
        // === Подключение к серверу через TCP ===
        SocketAddress a;
        printf("Resolving DNS for %s\n", hostname);
        int result = net->gethostbyname(hostname, &a);
        if (result != 0) 
        {
            printf("DNS resolution failed (%d), retrying...\n", result);
            ThisThread::sleep_for(15s);
            goto mar;
        }
        a.set_port(port);
        printf("Connecting to %s:%d\n", hostname, port);

        socket.open(net);
        int rc = socket.connect(a);
        if (rc != NSAPI_ERROR_OK) {
            printf("TCP connect error: %d, retrying...\n", rc);
            socket.close();
            ThisThread::sleep_for(15s);
            continue;
        }
        printf("Socket opened and connected.\n");

        // === Подключение к MQTT брокеру ===
        client.connect(data);
        if (!client.isConnected()) {
            printf("MQTT connection failed, retrying...\n");
            socket.close();
            ThisThread::sleep_for(15s);
            continue;
        }
        printf("Connected to MQTT broker.\n");

        // === Подписка на топик ===
        if ((rc = client.subscribe("sensor/set", MQTT::QOS0, messageArrived)) != 0) {
            printf("MQTT subscribe error: %d, reconnecting...\n", rc);
            client.disconnect();
            socket.close();
            ThisThread::sleep_for(15s);
            continue;
        }

        // === Основной цикл работы ===
        int elapsed = 0; 
        float temperature = sensor_bme.getTemperature();
        float pressure_pa = sensor_bme.getPressure(); 
        float pressure_mmHg = pressure_pa / 1.33322f;  
        float humidity = sensor_bme.getHumidity();
        float lux = sensor_opt.readSensor();           
        float co2 = mq135.getPPM();

        checkThresholdsAndControlTransistor(&client, temperature, humidity, pressure_mmHg, lux, co2);
        update_display(temperature, humidity, pressure_mmHg, lux, co2);

        while (true) 
        {            
            client.yield(1000);
            ThisThread::sleep_for(10s);
            elapsed += 10;
            
            if (!client.isConnected()) 
            {
                printf("MQTT connection lost. Reconnecting...\n");
                break;
            }
            
            if (elapsed >= 600) {
                float temperature = sensor_bme.getTemperature();
                float pressure_pa = sensor_bme.getPressure(); 
                float pressure_mmHg = pressure_pa / 1.33322f;  
                float humidity = sensor_bme.getHumidity();
                float lux = sensor_opt.readSensor();           
                float co2 = mq135.getPPM();

                checkThresholdsAndControlTransistor(&client, temperature, humidity, pressure_mmHg, lux, co2);
                update_display(temperature, humidity, pressure_mmHg, lux, co2);

                elapsed = 0; 
            }
        }        
        client.disconnect();
        socket.close();
        ThisThread::sleep_for(5s);
    }
}

int main() 
{      
    #ifdef MBED_MAJOR_VERSION
        printf("Mbed OS version %d.%d.%d\n", MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION);
    #endif
    printf("Samsung project ver. 1.1\n");
    
    display.begin(&i2c);
    display.clearDisplay();
    display.setFont(fontRus6x8);
    display.setCoding(Encoding::UTF8);
    display.print("Samsung project v.1.1", 0, 0);
    display.print("Инициализация...", 0, 10);
    display.updateAll();    

    mark2:
    wifi = WiFiInterface::get_default_instance();
    if (!wifi) 
    {
        printf("ERROR: No WiFiInterface found.\n");
        display.clearDisplay();
        display.print("WiFi не найден!", 0, 0);
        display.print("Повторный поиск...", 0, 10); 
        display.updateAll();
        ThisThread::sleep_for(10s);
        goto mark2;
    }

    mark3:
    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) 
    {
        printf("\nConnection error: %d\n", ret);
        display.clearDisplay();
        display.print("Ошибка подкл. к WiFi", 0, 0);        
        display.print("Повторное подкл...", 0, 10);                 
        display.updateAll();
        ThisThread::sleep_for(10s);
        goto mark3;
    }
    display.clearDisplay();
    display.print("Подкл. к WiFi усп.", 0, 0);
    display.updateAll();
    printf("Success\n\n");
    ThisThread::sleep_for(5s);
    printf("MAC: %s\n", wifi->get_mac_address());
    SocketAddress a;
    wifi->get_ip_address(&a);
    printf("IP: %s\n", a.get_ip_address());
    if (a.get_ip_address() == NULL)
    {
        goto mark2;
    }
    display.clearDisplay();
    display.print("Прогрев MQ-135...", 0, 0);
    display.updateAll();
    ThisThread::sleep_for(120s); 
    mq135.calibrate();
    printf("Calibration done. RZero = %.2f kΩ\r\n", mq135.getRZero());
    display.print("Калибровка усп...", 0, 10);
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "RZero = %.2f кОм", mq135.getRZero());
    display.print(buffer, 0, 20);    
    display.updateAll();
    ThisThread::sleep_for(5s);
    mqtt_demo(wifi);
    wifi->disconnect();
    printf("\nDone\n");
}