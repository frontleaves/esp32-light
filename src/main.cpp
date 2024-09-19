#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define GPIO_PIN12 12
#define GPIO_PIN13 13
#define GPIO_PIN14 14
#define GPIO_PIN27 27

// 全局变量
bool webControl = false;
bool isAuth = false;
bool isAuthCheck = false;
bool isMqttConnected = false;
bool isLightOn = false;

// WiFi 网络名称和密码
auto ssid = "Frontleaves";
auto password = "frontleaves114477";

// MQTT 服务器地址和端口
auto mqtt_server = "raspberrypi"; // 替换为树莓派的实际 IP 地址
constexpr int mqtt_port = 1883;

// 硬件配置
const std::string device_name = "xiaolfeng-light-b2ff80df74901114";
const std::string device_username = "c79593cbf548c388";
const std::string device_password = "638309a43fceecd1";

// 创建 WiFi 和 PubSubClient 客户端
WiFiClient espClient;
PubSubClient client(espClient);

// 任务句柄
TaskHandle_t Task1;
TaskHandle_t Task2;

void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi 已连接");
    Serial.println("IP 地址: ");
    Serial.println(WiFi.localIP());
}

void callback(const char *topic, const byte *payload, unsigned int length) {
    // 将 payload 转换为字符串
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += static_cast<char>(payload[i]);
    }
    Serial.println(message);

    // 解析 JSON 数据
    StaticJsonDocument<200> doc;
    const DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.print("JSON 反序列化失败：");
        Serial.println(error.f_str());
        return;
    }

    // 从 JSON 数据中提取字段
    const std::string action = doc["action"];
    const std::string value = doc["value"];

    // 检查是否登录
    if (action == "auth") {
        isAuthCheck = true;
        if (value == "true") {
            isAuth = true;
            Serial.println("[MQTT] 授权认证");
            digitalWrite(GPIO_PIN13, HIGH);
        }
    }
}

void reconnect() {
    while (!client.connected()) {
        digitalWrite(GPIO_PIN12, HIGH);
        Serial.println(WiFi.localIP());
        Serial.print("[MQTT] 尝试 MQTT 连接...");
        // 尝试连接
        if (client.connect(device_name.c_str())) {
            Serial.println("已连接");
            digitalWrite(GPIO_PIN12, LOW);
            // 连接成功后订阅主题
            client.subscribe("test/topic");
            isMqttConnected = true; // 确保标志位设置
        } else {
            Serial.print("连接失败, rc=");
            Serial.print(client.state());
            Serial.println(" 5 秒后重试");
            delay(5000);
        }
    }
}

[[noreturn]] void taskLightControl(void *pvParameters) {
    while (true) {
        if (!webControl) {
            if (digitalRead(GPIO_PIN27)) {
                digitalWrite(GPIO_PIN14, HIGH);
                if (isMqttConnected && isAuth && !isLightOn) {
                    client.publish("test/topic", R"({"type":"light","value":"on"})");
                }
                isLightOn = true;
            } else {
                digitalWrite(GPIO_PIN14, LOW);
                if (isMqttConnected && isAuth && isLightOn) {
                    client.publish("test/topic", R"({"type":"light","value":"off"})");
                }
                isLightOn = false;
            }
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(9600);

    pinMode(GPIO_PIN12, OUTPUT);
    pinMode(GPIO_PIN13, OUTPUT);
    pinMode(GPIO_PIN14, OUTPUT);
    pinMode(GPIO_PIN27, INPUT);
    digitalWrite(GPIO_PIN12, HIGH);
    digitalWrite(GPIO_PIN13, HIGH);
    digitalWrite(GPIO_PIN14, LOW);
    digitalWrite(GPIO_PIN27, LOW);

    delay(1000);

    digitalWrite(GPIO_PIN12, LOW);
    digitalWrite(GPIO_PIN13, LOW);

    // 连接到 WiFi
    setup_wifi();

    // 设置 MQTT 服务器
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    xTaskCreate(taskLightControl, "Task Light Control", 2048, nullptr, 1, &Task2); // 增加堆栈大小
}

void loop() {
    if (!client.connected()) {
        reconnect();
    } else {
        if (!isAuth) {
            const std::string value = R"({"device":")" + device_name + R"(","username":")" + device_username +
                                      R"(","password":")" + device_password + R"(","type":"auth"})";
            client.publish("test/topic", value.c_str());
            Serial.println("[MQTT] 已发送登录请求");
        }
    }
    client.loop();
    delay(500);
}
