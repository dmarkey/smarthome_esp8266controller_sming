#include <user_config.h>
#include <user_interface.h>
#include <SmingCore/SmingCore.h>

// Put you SSID and Password here
#define WIFI_SSID "access"
#define WIFI_PWD ""
#define WIFI_CONF_FILE "wifi_conf"

#define page "<html> <body> <form action='.' method='post'>  SSID: <input type='text' name='ssid'><br>  Password: <input type='text' name='password'><br>  <input type='submit' value='Submit'></form></body></html>"


#define _FOUR_SWITCH_MODE 0


void onMessageReceived(String topic, String message); // Forward declaration for our callback


#ifdef FOUR_SWITCH_MODE
    const int latchPin = 0;
    //Pin connected to clock pin (SH_CP) of 74HC595
    const int clockPin = 2;
    ////Pin connected to Data in (DS) of 74HC595

    const int dataPin = 3;

    int register_state = 0;

#else
    int switches[] = {0, 2};
#endif

HttpClient hc;

Timer procTimer;



HttpServer server;

bool httpInProgress = false;


String mqttName(){
    String name;
    int id = system_get_chip_id();
    name = "SmartController-";
    name  += id;
    return name;
}


String commandTopic(){
    String topic;
    int id = system_get_chip_id();
    topic = "/smart_plug_work/SmartPlug-";
    topic  = topic + id;
    return topic;
}

class ReconnectingMqttClient: public MqttClient{
    using MqttClient::MqttClient; // Inherit Base's constructors.

    void onError(err_t err)  {
        close();
        connect(mqttName());
        subscribe(commandTopic());
        return;
    }

};

ReconnectingMqttClient mqtt("dmarkey.mooo.com", 8000, onMessageReceived);

void printResponse(HttpClient& hc, bool success){
    Serial.print(hc.getResponseString());
}


#ifdef FOUR_SWITCH_MODE
void ICACHE_FLASH_ATTR push_to_register()
{
    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, MSBFIRST, register_state);
    digitalWrite(latchPin, HIGH);

}
#endif // FOUR_SWITCH_MODE


void ICACHE_FLASH_ATTR set_switch(int swit, bool state)
{
    swit--;
    Serial.print("Setting switch ");
    Serial.print(swit);
    Serial.print(" to ");
    Serial.print(state);
    #ifdef FOUR_SWITCH_MODE
        int i;
        swit--;
        Serial.print("Setting switch ");
        Serial.print(swit);
        Serial.print(" to ");
        Serial.print(state);
        Serial.println();
        bitWrite(register_state, swit, state);
        //Serial.println(register_state, BIN);
        push_to_register();
    #else

        digitalWrite(switches[swit], !state);
    #endif
}


void processSwitchcmd(JsonObject& obj){
    int switch_num = obj["socketnumber"];
    bool state = obj["state"];
    set_switch(switch_num, state);

}


void setTaskStatus(JsonObject& obj, int status){

    char post_data[256];
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["controller_id"] = system_get_chip_id();
    root["task_id"] = obj["task_id"];
    root["status"] = status;

    root.printTo(post_data, sizeof(post_data));
    mqtt.publish("/admin/task_status", post_data);
    //queue_web_request("http://dmarkey.com:8080/controller_task_status/", post_data, "application/json");
}

void beaconComplete(){

    StaticJsonBuffer<200> jsonBuffer;
    char data[200];
    JsonObject& root = jsonBuffer.createObject();
    root["controller_id"] = system_get_chip_id();
    root.printTo(data, sizeof(data));
    mqtt.publish("/admin/beacon", data);
    //queue_web_request("http://dmarkey.com:8080/controller_task_status/", post_data, "application/json");
}


// Callback for messages, arrived from MQTT server
void onMessageReceived(String topic, String message)
{

	Serial.print(topic);
	if (topic == commandTopic()){
	    StaticJsonBuffer<200> jsonBuffer;
        const char *json = message.c_str();
        JsonObject& root = jsonBuffer.parseObject((char *)json);

        const char * command = root["name"];
        setTaskStatus(root, 2);

        if (strcmp(command, "sockettoggle") != -1){
                processSwitchcmd(root);
                setTaskStatus(root, 3);

        }


	}

}




void processBeaconResponse(HttpClient& hc, bool success){
    Serial.print(hc.getResponseString());
    if (success){
        mqtt.connect(mqttName());
        mqtt.subscribe(commandTopic());
        beaconComplete();
    }



    //processing_web = false;
    //process_web();
}


void beaconFunc(){
    String post_data;
    post_data = "model=SmartController-2&controller_id=";
    post_data += system_get_chip_id();
    post_data += "\r\n";
    //queue_web_request("http://dmarkey.com:8080/controller_ping_create/", post_data);
    hc.setPostBody(post_data);
    hc.downloadString("http://dmarkey.com:8080/controller_ping_create/", processBeaconResponse);
}



// Will be called when WiFi station was connected to AP


void connectOk()
{
	Serial.println("I'm CONNECTED");
    beaconFunc();
	// Run MQTT client

}

void writeConf(String SSID, String Pwd){
    String buf;
    char cstring[100];
    buf = SSID + "\n" + Pwd;
    buf.toCharArray(cstring, 100);
    fileSetContent(WIFI_CONF_FILE, cstring);
}


void restart(){
    System.restart();
}

void onIndex(HttpRequest &request, HttpResponse &response)
{
	String ssid = request.getPostParameter("ssid");
	String password = request.getPostParameter("password");

	if (ssid == ""){
        response.sendString(page);
        return;
	}
	else{
        writeConf(ssid, password);
        response.sendString("Success");
        procTimer.initializeMs(1000, restart).start();
	}

}

// Will be called when WiFi station timeout was reached
void connectFail()
{
    String SSID;
    SSID = "Smarthome-";
    SSID = SSID + system_get_chip_id();
    WifiAccessPoint.config(SSID,"", AUTH_OPEN, false, 2, 2000);
    WifiAccessPoint.enable(true);
    server.listen(80);
	server.addPath("/", onIndex);
	WifiStation.enable(false);

	Serial.println("Fallback WIFI mode.");

	// .. some you code for device configuration ..
}


void init()
{
#ifdef FOUR_SWITCH_MODE
    pinMode(0, OUTPUT);
    digitalWrite(0, LOW);
    pinMode(3, OUTPUT);
    digitalWrite(3, LOW);
    pinMode(latchPin, OUTPUT);
	pinMode(dataPin, OUTPUT);
	pinMode(clockPin, OUTPUT);
    push_to_register();

#else // FOUR_SWITCH_MODE
    pinMode(0, OUTPUT);
    pinMode(2, OUTPUT);
#endif

	Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
	Serial.systemDebugOutput(true); // Debug output to serial


	//wifi_station_set_hostname("MyEsp8266");

	String wifiSSID, wifiPassword, tmp;
	file_t wifi_file;



	if(!fileExist(WIFI_CONF_FILE)){
        writeConf(WIFI_SSID, WIFI_PWD);

	}
	else{
        tmp = fileGetContent(WIFI_CONF_FILE);
        Serial.println("WIFI_CONFIG");
        Serial.println(tmp);
        int newline = tmp.indexOf('\n');
        wifiSSID = tmp.substring(0, newline);
        wifiPassword = tmp.substring(newline+1, tmp.length());
        Serial.println(wifiSSID);
        Serial.println(wifiPassword);
        //wifiSSID = fileRead(wifi_file);


	}

	WifiStation.config(wifiSSID, wifiPassword);
	WifiStation.enable(true);
	WifiAccessPoint.enable(false);

	// Run our method when station was connected to AP (or not connected)
	WifiStation.waitConnection(connectOk, 20, connectFail); // We recommend 20+ seconds for connection timeout at start
}
