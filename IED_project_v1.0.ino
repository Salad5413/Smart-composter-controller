#include <SoftwareSerial.h>
#include <arduino-timer.h>
#include <dht.h>


#define dht_apin A0 // Analog Pin sensor is connected to
 
dht DHT;
SoftwareSerial wifiSerial(3, 2); // RX, TX for ESP8266

bool DEBUG = false;   //show more logs
int responseTime = 10; //communication timeout
int temperature_Limit = 80; // 
float f = 0;
bool f_too_high = false;

// Output ports
int startSignal = 8;

// Send temperature & humidity data to TCP every 2 second ( >= 2 seconds between measurements )
unsigned long delay_time = 2000; // millisecond
auto send_data_timer = timer_create_default();
//auto buzzer_on_timer = timer_create_default();

void setup() {
///////////////////////////////////////////////////////////////////////////////////
  // Open serial communications and wait for port to open esp8266:
  Serial.begin(9600);
  Serial.println("Serial Setted");
  wifiSerial.begin(115200);
  wifiSerial.println("wifiSerial Setted");
///////////////////////////////////////////////////////////////////////////////////
  
  sendToWifi("AT+CWMODE=2",responseTime,DEBUG); // configure as access point
  sendToWifi("AT+CIFSR",responseTime,DEBUG); // get ip address
  sendToWifi("AT+CIPMUX=1",responseTime,DEBUG); // configure for multiple connections
  sendToWifi("AT+CIPSERVER=1,80",responseTime,DEBUG); // turn on server on port 80
///////////////////////////////////////////////////////////////////////////////////
  // Output
  // Start/Stop Signal Output
  pinMode(startSignal,OUTPUT);  //set PIN 8(connected to red LED) as output
  pinMode(7, OUTPUT);
  pinMode(5, INPUT); // set PIN 5 as input (Latch state, 0 = door is open, 1 = door is closed)
  // Send temperature & humidity data to TCP every "delay_time" second
  // Data type: String; 
  // Format: "(temperature)" + "-" + "(humidity)"
  send_data_timer.every(delay_time+500, sendTHdata);
  send_data_timer.every(1500, sendLatchdata);
  send_data_timer.every(2000, buzzer_on);

}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(7,HIGH);
  // Tick the timer
  send_data_timer.tick(); // tick the timer
  
  // Message = Arduino received messages from TCP
  if(wifiSerial.available()>0){
    String message = readWifiSerialMessage();
    message = message.substring(9);
    Serial.println(message);
    
    // Response after receiving a message
     if(message == "")
     {;}
     else if(message == "test")
     {
        sendData("Connection succeeded\n");
     }
    /////////////////////////////////////////////////////////////////////
     else if(message == "On")
     {
        if(f_too_high == false)
        {
          digitalWrite(startSignal,HIGH);
        }
     }
     else if(message == "Off")
     {
        digitalWrite(startSignal,LOW);
     }
    /////////////////////////////////////////////////////////////////////
     else
     {
        sendData("Unknown message\n");
     }
  }

  /////////////////// Latch ///////////////////////////
  if(digitalRead(5) == 0)
  {
    digitalWrite(startSignal,LOW); // If door is open, emergency stop
  }
}


/*
* Name: buzzer_on
* Description: Function used to turn on the buzzer
* Params: void
* Returns: void
*/
bool buzzer_on(void *){
  if(f_too_high == false)
  {
    noTone(12);
  }
  else
  {
    tone(12,1400,1000);
  }
  return true;
}


/*
* Name: sendLatchdata
* Description: Function used to send the state of door
* Params: void
* Returns: void
*/
bool sendLatchdata(void *){
  int doorState = digitalRead(5);
  
  //Serial.println(doorState);
  
  if(doorState == 0)
  {
    sendData("doorIsOpen,-," + String(f_too_high));
  }
  else if(doorState == 1)
  {
    sendData("doorIsClosed,-," + String(f_too_high));
  }
  return true;
}


/*
* Name: sendTHdata
* Description: Function used to send temperature & humidity data to TCP
* Params: void
* Returns: void
*/
bool sendTHdata(void *){
  // Read the temperature & humidity value
  DHT.read11(dht_apin);
  float h = DHT.humidity;
  f = DHT.temperature*1.8 + 32;  // Read temperature as Fahrenheit (isFahrenheit = true)
  String send_TH_data = String("Err") + "," + String("Err") + "," + String(f_too_high);

  if(f>temperature_Limit){
    Serial.println(String(f) + "," + String(h) + ", temperature is too high!");
    f_too_high = true;
    digitalWrite(startSignal,LOW);
    send_TH_data = String(f) + "," + String(h) + "," + String(f_too_high);
    sendData(send_TH_data); 
    //tone(12,1400);
    //delay(1000);
    return true;
  }
  else{
    f_too_high = false;
    //noTone(12);
  }

  String f_string = String(f);
  //f_string = String(f_string);
  String h_string = String(h);
  //h_string = String(h_string);

  Serial.println(f_string + "," + h_string + "," + String(f_too_high)); //============================================

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(f)) 
  {
    // Send the error data
    send_TH_data = String("Err") + "," + String("Err") + "," + String(f_too_high);
    sendData(send_TH_data);   
    return true;
  }


  // Send the data
  send_TH_data = f_string + "," + h_string + "," + String(f_too_high);
  sendData(send_TH_data);
  
  return true;
}


/*
* Name: sendData
* Description: Function used to send string to tcp client using cipsend
* Params: String str
* Returns: void
*/
void sendData(String str){
  //String len="";
  //len+=str.length();
  sendToWifi("AT+CIPSEND=0,"+String(str.length()),responseTime,DEBUG);
  delay(100);
  sendToWifi(str,responseTime,DEBUG);
}


/*
* Name: readWifiSerialMessage
* Description: Function used to read data from ESP8266 Serial.
* Params: 
* Returns: The response from the esp8266 (if there is a reponse)
*/
String  readWifiSerialMessage(){
  char value[100]; 
  int index_count =0;
  while(wifiSerial.available()>0){
    value[index_count]=wifiSerial.read();
    index_count++;
    value[index_count] = '\0'; // Null terminate the string
  }
  String str(value);
  str.trim();
  return str;
}

/*
* Name: sendToWifi
* Description: Function used to send data to ESP8266.
* Params: command - the data/command to send; timeout - the time to wait for a response; debug - print to Serial window?(true = yes, false = no)
* Returns: The response from the esp8266 (if there is a reponse)
*/
String sendToWifi(String command, const int timeout, boolean debug){
  String response = "";
  wifiSerial.println(command); // send the read character to the esp8266
  long int time = millis();
  while( (time+timeout) > millis())
  {
    while(wifiSerial.available())
    {
    // The esp has data so display its output to the serial window 
    char c = wifiSerial.read(); // read the next character.
    response+=c;
    }  
  }
  if(debug)
  {
    Serial.println(response);
  }
  return response;
}
