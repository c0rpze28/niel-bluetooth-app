#include "BLE_NUS_Service.h"
#include <Arduino.h>

// Constructor
BLE_NUS_Service::BLE_NUS_Service() : commandCallback(nullptr) {}

// Initialize the BLE service
void BLE_NUS_Service::begin(const char* deviceName, 
                            std::function<void(const std::string&)> onCommandCallback,
                            bool enableEncryption,
                            uint32_t passKey) {
    
    if (initialized) return;
    
    commandCallback = onCommandCallback;
    securityPassKey = passKey;
    
    // Initialize NimBLE
    NimBLEDevice::init(deviceName);
    
    // Power saving mode (adjust based on your needs)
    NimBLEDevice::setPower(ESP_PWR_LVL_N0); // Lowest power
    
    if (enableEncryption) {
        NimBLEDevice::deleteAllBonds();
        NimBLEDevice::setSecurityAuth(false, false, true); // Bonding, MITM, Secure connections
    }
    
    // Create server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks(this));
    pServer->advertiseOnDisconnect(true);
    
    // Create service
    NimBLEService* pService = pServer->createService(NUS_SERVICE_UUID);
    
    // RX Characteristic (Write)
    pRxCharacteristic = pService->createCharacteristic(
        NUS_RX_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE |
        (enableEncryption ? NIMBLE_PROPERTY::WRITE_ENC : 0));
    
    if (commandCallback) {
        pRxCharacteristic->setCallbacks(new RxCallbacks(this));
    }
    
    // TX Characteristic (Notify)
    pTxCharacteristic = pService->createCharacteristic(
        NUS_TX_CHAR_UUID,
        NIMBLE_PROPERTY::NOTIFY |
        (enableEncryption ? NIMBLE_PROPERTY::NOTIFY_ENC : 0));
    
    // Add CCCD descriptor for notifications
    NimBLEDescriptor* cccd = new NimBLEDescriptor("2902", 0, 2, pTxCharacteristic);
    uint8_t cccd_value[2] = { 0x00, 0x00 };
    cccd->setValue(cccd_value, 2);
    pTxCharacteristic->addDescriptor(cccd);
    
    // Start service
    pService->start();
    
    // Setup advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(NUS_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinInterval(32); // 20ms
    pAdvertising->setMaxInterval(64); // 40ms
    
    // Start advertising
    NimBLEDevice::startAdvertising();
    
    initialized = true;
    
    Serial.println("BLE NUS Service initialized");
}

// Send string data
bool BLE_NUS_Service::sendData(const std::string& data) {
    if (!initialized || !pTxCharacteristic || !connected) {
        return false;
    }
    
    pTxCharacteristic->setValue(data);
    pTxCharacteristic->notify();
    return true;
}

// Send C-string data
bool BLE_NUS_Service::sendData(const char* data) {
    return sendData(std::string(data));
}

// Send float data
bool BLE_NUS_Service::sendData(float value, int precision) {
    char buffer[32];
    
    if (precision == 0) {
        snprintf(buffer, sizeof(buffer), "%d", (int)value);
    } else {
        char format[8];
        snprintf(format, sizeof(format), "%%.%df", precision);
        snprintf(buffer, sizeof(buffer), format, value);
    }
    
    return sendData(std::string(buffer));
}

// Send integer data
bool BLE_NUS_Service::sendData(int value) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return sendData(std::string(buffer));
}

// Check connection status
bool BLE_NUS_Service::isConnected() {
    return connected;
}

// Get number of connected clients
int BLE_NUS_Service::getConnectedCount() {
    if (!initialized || !pServer) return 0;
    return pServer->getConnectedCount();
}

// Start advertising
void BLE_NUS_Service::startAdvertising() {
    if (!initialized) return;
    NimBLEDevice::startAdvertising();
}

// Stop advertising
void BLE_NUS_Service::stopAdvertising() {
    if (!initialized) return;
    NimBLEDevice::getAdvertising()->stop();
}

// Clean up
void BLE_NUS_Service::end() {
    if (!initialized) return;
    
    // Disconnect all clients
    if (pServer) {
        pServer->disconnectAll();
    }
    
    // Deinit NimBLE
    NimBLEDevice::deinit();
    
    initialized = false;
    connected = false;
}

// ========== Callback Implementations ==========

void BLE_NUS_Service::RxCallbacks::onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    
    if (!value.empty() && parent->commandCallback) {
        parent->commandCallback(value);
    }
}

void BLE_NUS_Service::ServerCallbacks::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    parent->connected = true;
    Serial.println("BLE Client connected");
    
    // Stop advertising while connected
    NimBLEDevice::getAdvertising()->stop();
}

void BLE_NUS_Service::ServerCallbacks::onDisconnect(NimBLEServer* pServer, 
                                                     NimBLEConnInfo& connInfo, 
                                                     int reason) {
    parent->connected = false;
    Serial.print("BLE Client disconnected. Reason: ");
    Serial.println(reason);
    
    // Restart advertising after disconnection
    delay(500);
    NimBLEDevice::startAdvertising();
}

uint32_t BLE_NUS_Service::ServerCallbacks::onPassKeyRequest() {
    Serial.print("Passkey requested, sending: ");
    Serial.println(parent->securityPassKey);
    return parent->securityPassKey;
}

void BLE_NUS_Service::ServerCallbacks::onAuthenticationComplete(NimBLEConnInfo& connInfo) {
    if (!connInfo.isEncrypted()) {
        Serial.println("Encryption failed - disconnecting");
        parent->pServer->disconnect(connInfo.getConnHandle());
        parent->connected = false;
    } else {
        Serial.println("Authentication successful");
    }
}