#include "SmartRC_CC1101.h"

SmartRC_CC1101 radio;

void setup() {
    Serial.begin(115200);
    radio.Init();
    
    if(radio.getCC1101()) {
        Serial.println("Hardware found and ready!");
    } else {
        Serial.println("Hardware Not found.");
    }
}

void loop() {

}
