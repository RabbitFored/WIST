#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// These are unique ID numbers for W.I.S.T. so other devices can recognize it
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
uint32_t heartbeat = 0;

// This class listens for connections and disconnections
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("--- CONNECTION ESTABLISHED ---");
    };
    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("--- CONNECTION LOST ---");
        Serial.println("Restarting BLE broadcast...");
        BLEDevice::startAdvertising(); // Keep broadcasting so you can reconnect
    }
};

void setup() {
    Serial.begin(115200);
    Serial.println("Starting W.I.S.T. BLE Diagnostic...");

    // 1. Initialize the BLE antenna and name the device
    BLEDevice::init("W.I.S.T_TEST");

    // 2. Create the Server (The Watch itself)
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // 3. Create the Service (The specific capability, like "Telemetry")
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // 4. Create the Characteristic (The actual data channel we write to)
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ   |
                        BLECharacteristic::PROPERTY_WRITE  |
                        BLECharacteristic::PROPERTY_NOTIFY
                    );

    // Set the initial value
    pCharacteristic->setValue("W.I.S.T. Online");

    // 5. Start the service and turn on the beacon
    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();
    
    Serial.println("BLE Broadcast Active. Waiting for a scanner...");
}

void loop() {
    if (deviceConnected) {
        // Create a fake heartbeat signal to prove data is moving
        char txString[32];
        sprintf(txString, "Vitals: %d BPM", heartbeat);
        
        // Write it to the BLE channel and push it to the phone
        pCharacteristic->setValue(txString);
        pCharacteristic->notify(); 
        
        Serial.print("Transmitting: ");
        Serial.println(txString);
        
        heartbeat++;
    }
    
    // Transmit once per second
    delay(1000); 
}