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
    
    // Power saving mode
    NimBLEDevice::setPower(ESP_PWR_LVL_N0);
    
    if (enableEncryption) {
        NimBLEDevice::deleteAllBonds();
        NimBLEDevice::setSecurityAuth(false, false, true);
    }
    
    // Create server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks(this));
    pServer->advertiseOnDisconnect(true);
    
    // Create service
    NimBLEService* pService = pServer->createService(NUS_SERVICE_UUID);
    
    // RX Characteristic (Write)
    uint32_t rxProperties = NIMBLE_PROPERTY::WRITE;
    if (enableEncryption) {
        rxProperties |= NIMBLE_PROPERTY::WRITE_ENC;
    }
    pRxCharacteristic = pService->createCharacteristic(
        NUS_RX_CHAR_UUID,
        rxProperties);
    
    if (commandCallback) {
        pRxCharacteristic->setCallbacks(new RxCallbacks(this));
    }
    
    // TX Characteristic (Notify) - NOTIFY_ENC might not exist
    uint32_t txProperties = NIMBLE_PROPERTY::NOTIFY;
    // Skip NOTIFY_ENC if it doesn't exist
    pTxCharacteristic = pService->createCharacteristic(
        NUS_TX_CHAR_UUID,
        txProperties);
    
    // Add CCCD descriptor for notifications - using the 4-parameter constructor
    NimBLEDescriptor* cccd = new NimBLEDescriptor(
        NimBLEUUID((uint16_t)0x2902),
        0,  // properties
        2,  // max length
        pTxCharacteristic
    );
    uint8_t cccd_value[2] = { 0x00, 0x00 };
    cccd->setValue(cccd_value, 2);
    pTxCharacteristic->addDescriptor(cccd);
    
    // Start service
    pService->start();
    
    // Setup advertising - use setScanResponseData instead of setScanResponse
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(NUS_SERVICE_UUID);
    // pAdvertising->setScanResponse(true); // Comment out if not available
    
    // Start advertising
    pAdvertising->start();
    
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
    NimBLEDevice::getAdvertising()->start();
}

// Stop advertising
void BLE_NUS_Service::stopAdvertising() {
    if (!initialized) return;
    NimBLEDevice::getAdvertising()->stop();
}

// Clean up
void BLE_NUS_Service::end() {
    if (!initialized) return;
    
    // disconnectAll might not exist, use disconnect on each connection
    if (pServer) {
        // Get connected count and disconnect each
        int count = pServer->getConnectedCount();
        for (int i = 0; i < count; i++) {
            pServer->disconnect(i);
        }
    }
    
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

void BLE_NUS_Service::RxCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, const uint8_t* data, size_t len) {
    std::string value(reinterpret_cast<const char*>(data), len);
    
    if (!value.empty() && parent->commandCallback) {
        parent->commandCallback(value);
    }
}

void BLE_NUS_Service::ServerCallbacks::onConnect(NimBLEServer* pServer) {
    parent->connected = true;
    Serial.println("BLE Client connected");
    NimBLEDevice::getAdvertising()->stop();
}

void BLE_NUS_Service::ServerCallbacks::onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
    parent->connected = true;
    Serial.println("BLE Client connected");
    NimBLEDevice::getAdvertising()->stop();
}

void BLE_NUS_Service::ServerCallbacks::onDisconnect(NimBLEServer* pServer) {
    parent->connected = false;
    Serial.println("BLE Client disconnected");
    delay(500);
    NimBLEDevice::getAdvertising()->start();
}

void BLE_NUS_Service::ServerCallbacks::onDisconnect(NimBLEServer* pServer, ble_gap_conn_desc* desc, int reason) {
    parent->connected = false;
    Serial.println("BLE Client disconnected");
    delay(500);
    NimBLEDevice::getAdvertising()->start();
}

uint32_t BLE_NUS_Service::ServerCallbacks::onPassKeyRequest() {
    Serial.print("Passkey requested, sending: ");
    Serial.println(parent->securityPassKey);
    return parent->securityPassKey;
}

void BLE_NUS_Service::ServerCallbacks::onAuthenticationComplete(ble_gap_conn_desc* desc) {
    if (!desc->sec_state.encrypted) {
        Serial.println("Encryption failed - disconnecting");
        parent->pServer->disconnect(desc->conn_handle);
        parent->connected = false;
    } else {
        Serial.println("Authentication successful");
    }
}