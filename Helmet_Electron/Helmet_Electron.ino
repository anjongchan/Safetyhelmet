#include <ParticleSoftSerial.h>
#include <HttpClient.h>
#include <AssetTracker.h>
#include <Adafruit_BMP085.h>

#if !defined(PARTICLE)
 #include <Wire.h>
#endif

#if (SYSTEM_VERSION >= 0x00060000)
  SerialLogHandler logHandler;
#endif

#define SENDER          Serial1
#define RECEIVER        SoftSer
#define PROTOCOL        SERIAL_8N1
#define EMR_PIN         D7
#define RES_PIN         D5
#define FSR_PIN         A0
#define GAS_PIN         A1
#define SPE_PIN         B0
#define PSS_RX          D2 // RX must be interrupt enabled (on Photon/Electron D0/A5 are not)
#define PSS_TX          D3
#define WEB_UPDATE_TIME 3
#define LOC_UPDATE_TIME 1

ParticleSoftSerial SoftSer(PSS_RX, PSS_TX);
Adafruit_BMP085 bmp;
// Connect SCL to i2c clock - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 5, Particle on D1
// Connect SDA to i2c data - on '168/'328 Arduino Uno/Duemilanove/etc thats Analog 4, Particle on D0

int transmittingData = 1;   // Used to keep track of the last time we published data
long lastPublish = 0;       // How many minutes between publishes? 10+ recommended for long-time continuous publishing!
AssetTracker t = AssetTracker(); // Creating an AssetTracker named 't' for us to reference

unsigned int nextTime = 0;    // Next time to contact the server
HttpClient http;
// Headers currently need to be set at init, useful for API keys etc.
http_header_t headers[] = {
    { "Content-Type", "application/json" },
    { "Accept" , "application/json" },
    //{ "Accept" , "*/*"},
    { NULL, NULL } // NOTE: Always terminate headers will NULL
};
http_request_t request;
http_response_t response;

typedef struct _State {
    unsigned short pressure_alram:1;
    unsigned short pulse_alram:1;
    unsigned short fsr_alram:1;
    unsigned short emr_alram:1;
    unsigned short gas_alram:1;
    unsigned short Reset_EMR:1;
    unsigned short Reset_RES:1;
    unsigned short send:1;
} State;

State S1 = { 0, };

char szTX[64];
char szRX[64];
char Information[150];
char Information_state[15];
char string_la[15];
char string_lo[15];
char SUPADUPA[25];
char DUPADUPA[25];
char BPM_buff[5];
char altitude_buff[7];
int fsr_value;     // the analog reading from the FSR resistor divider
int gas_value;
int altitude_Helmet;   //helemet's Altitude Value
int altitude_Ring;     //ring's Altitude Value
int C;
int i;
int BPM = 90;
int BPM_alram_count = 0;
int alti_alram_count = 0;
void setup()
{
    S1.Reset_RES = digitalRead(RES_PIN);
    S1.Reset_EMR = digitalRead(EMR_PIN);
    S1.send = 1;
    strcpy(DUPADUPA, "49004c000351353337353037");
    strcpy(SUPADUPA, "4a0028000d51353432393339");

    request.hostname = "www.bobsafety.n-e.kr";  
    request.port = 8088;
    request.path = "/getData.do";
    
    // Sets up all the necessary AssetTracker bits
    t.begin();
    // Enable the GPS module. Defaults to off to save power.
    // Takes 1.5s or so because of delays.
    t.gpsOn();

    // These three functions are useful for remote diagnostics. Read more below.
    Particle.function("tmode", transmitMode);
    Particle.function("gps", gpsPublish);
  
    Serial.begin(9600);
    pinInit();

    if (!bmp.begin()) {
        Serial.println("Could not find a valid BMP085 sensor, check wiring!");
        while (!bmp.begin()) {}
    }
    RECEIVER.begin(9600);
}

void loop()
{
    Information[0] = '\0';
    gas_value = analogRead(GAS_PIN);
    fsr_value = analogRead(FSR_PIN);
    altitude_Helmet = bmp.readAltitude();
    
    Serial.print("     B count:");
    Serial.print(BPM_alram_count);
    Serial.print("     A count:");
    Serial.print(alti_alram_count);
    Serial.print("     gas:");
    Serial.print(gas_value);
    Serial.print("     fsr:");
    Serial.println(fsr_value);
    Serial.print("     BPM:");
    Serial.print(BPM);
    Serial.print("     ring--helmet:");
    Serial.print(altitude_Ring);
    Serial.print("//");
    Serial.println(altitude_Helmet);
    
    
    if(SoftSer.available()){
        BluetoothRead();
    }
    AlramCheck();
    EmergencyCheck(digitalRead(EMR_PIN));
    ResetCheck(digitalRead(RES_PIN));
    t.updateGPS();
            
    if((S1.fsr_alram != 1) && (S1.gas_alram != 1) && (S1.pulse_alram != 1) && (S1.pressure_alram != 1) && (S1.emr_alram != 1)) {
        strcpy(Information_state, "Location");
    }

    LocationUpdate(LOC_UPDATE_TIME);
    InformationUpdate();
    
    if((S1.fsr_alram == 1) || (S1.gas_alram == 1) || (S1.pulse_alram == 1) || (S1.pressure_alram == 1) || (S1.emr_alram == 1)) {
        if(S1.send == 1){
            Serial.println("**************  Warning !! ****************");
            tone(SPE_PIN, 2551, 3000);
            request.body = Information;    
            http.get(request, response, headers);
            Serial.println(Information);
            Serial.println("-------------------------------------------");
            S1.send = 0;
            SoftSer.flush();
        } else {
            tone(SPE_PIN, 2551, 1000);
        }
    } else {
       WebUpdate(WEB_UPDATE_TIME);
    }
    delay(100);
}


void pinInit()
{
  pinMode(SPE_PIN, OUTPUT);
  digitalWrite(SPE_PIN, LOW);
  pinMode(RES_PIN, INPUT);
  pinMode(EMR_PIN, INPUT);
}

// Allows you to remotely change whether a device is publishing to the cloud
// or is only reporting data over Serial. Saves data when using only Serial!
// Change the default at the top of the code.
int transmitMode(String command) {
    transmittingData = atoi(command);
    return 1;
}
// Actively ask for a GPS reading if you're impatient. Only publishes if there's
// a GPS fix, otherwise returns '0'
int gpsPublish(String command) {
    if(t.gpsFix()) {
        Particle.publish("G", t.readLatLon(), 60, PRIVATE);
        // uncomment next line if you want a manual publish to reset delay counter
        // lastPublish = millis();
        return 1;
    } else {
        return 0;
    }
}
void BluetoothRead() {
    if((C = SoftSer.read()) == 'B'){
        while((C = SoftSer.read()) == 'B') ;
        for(i = 0; ('0' <= C && C <= '9'); i++){
            BPM_buff[i] = C;
            C = SoftSer.read();
        }
        BPM_buff[i] = '\0';
        BPM = atoi(BPM_buff);
        while((C = SoftSer.read()) == 'S') ;
        for(i = 0; ((C == '-') || ('0'<= C && C <= '9')); i++){
            altitude_buff[i] = C;
            C = SoftSer.read();
        }
        altitude_buff[i] = '\0';
        altitude_Ring = atoi(altitude_buff);
        while((C = SoftSer.read()) == 'E') ;
        
        if(BPM <= 30) { BPM_alram_count++;
        } else BPM_alram_count = 0; 
        if(abs(altitude_Ring - altitude_Helmet) > 5) { alti_alram_count++;
        } else alti_alram_count = 0;
    }
}
void AlramCheck() {
    if(BPM_alram_count > 3) {
        S1.pulse_alram = 1;
        strcpy(Information_state, "pulse");
    } else if (fsr_value >= 2500) {
        S1.fsr_alram = 1;
        strcpy(Information_state, "Fsr");
    } else if (gas_value >= 1200) {
        S1.gas_alram = 1;
        strcpy(Information_state, "Gas");
    } else if (alti_alram_count > 3) {
        Serial.println("Warning !! Keep safety rings nearby");
        S1.pressure_alram = 1;
        strcpy(Information_state, "Ring");
    }
}
void EmergencyCheck(int State_RES) {
    if(State_RES != S1.Reset_EMR) {
        S1.emr_alram = 1;
        strcpy(Information_state, "Emergency");
        S1.Reset_EMR = ~S1.Reset_EMR;
    }
}
void ResetCheck(int State_EMR) {
    if(State_EMR != S1.Reset_RES) {
        Serial.println("------ alram Reset ------");
        S1.fsr_alram = 0;
        S1.gas_alram = 0;
        S1.pulse_alram = 0;
        S1.emr_alram = 0;
        S1.send = 1;
        S1.Reset_RES = ~S1.Reset_RES;
        S1.pressure_alram =0;
    }
}
void LocationUpdate(int G_delayMinutes) {
    if(G_delayMinutes*60*1000 < (millis()-lastPublish)){
      // Remember when we published
        lastPublish = millis();
        //Particle.publish("A", pubAccel, 60, PRIVATE);
        // Dumps the full NMEA sentence to serial in case you're curious
        //Serial.println(t.preNMEA());
        String pubAccel1 = String::format("%f", t.readLatDeg());
        strcpy(string_la, pubAccel1);
        String pubAccel2 = String::format("%f", t.readLonDeg());
        strcpy(string_lo, pubAccel2);
        Serial.println("GPS update");
        Serial.println("-------------------------------------------");        
        // GPS requires a "fix" on the satellites to give good data,
        // so we should only publish data if there's a fix
        if(t.gpsFix()) {
            // Only publish if we're in transmittingData mode 1;
            if(transmittingData) {
                // Short publish names save data!
                Particle.publish("G", t.readLatLon(), 60, PRIVATE);
            }
        }
    } 
}
void WebUpdate(int W_delayMinutes) {
    if((W_delayMinutes*60*1000) < (millis() - nextTime)) {
        nextTime = millis();    
        Serial.println();
        Serial.println("Sending status messaes every three minutes");
        //The library also supports sending a body with your request:
        request.body = Information;    
        http.get(request, response, headers);
        Serial.print("Application>\tResponse status: ");
        Serial.println(response.status);
        Serial.print("Application>\tHTTP Response Body: ");
        Serial.println(response.body);
        Serial.println(Information);
        Serial.println("-------------------------------------------");
        SoftSer.flush();
    }
}
void InformationUpdate() {
    strcat(Information, "{\"deviceId\":");
    strcat(Information, DUPADUPA);
    strcat(Information, ",\"kind\":\"");
    strcat(Information, Information_state);
    strcat(Information, "\",\"gpsLatitude\":\"");
    strcat(Information, string_la);
    strcat(Information, "\",\"gpsLongitude\":\"");
    strcat(Information, string_lo);
    strcat(Information, "\"}");
}
