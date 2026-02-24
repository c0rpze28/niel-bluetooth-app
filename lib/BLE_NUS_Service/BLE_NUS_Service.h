#ifndef BLE_NUS_SERVICE_H
#define BLE_NUS_SERVICE_H

#include <NimBLEDevice.h>
#include <functional>
#include <string>

// NUS Service UUIDs (Nordic UART Service)
#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHAR_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_CHAR_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class BLE_NUS_Service {
public:
    // Constructor
    BLE_NUS_Service();
    
    // Initialize the BLE service
    void begin(const char* deviceName, 
               std::function<void(const std::string&)> onCommandCallback = nullptr,
               bool enableEncryption = true,
               uint32_t passKey = 123456);
    
    // Send data to connected client
    bool sendData(const std::string& data);
    bool sendData(const char* data);
    bool sendData(float value, int precision = 1);
    bool sendData(int value);
    
    // Check connection status
    bool isConnected();
    
    // Get number of connected clients
    int getConnectedCount();
    
    // Manually control advertising
    void startAdvertising();
    void stopAdvertising();
    
    // Clean up
    void end();

private:
    NimBLEServer* pServer = nullptr;
    NimBLECharacteristic* pRxCharacteristic = nullptr;
    NimBLECharacteristic* pTxCharacteristic = nullptr;
    
    std::function<void(const std::string&)> commandCallback;
    bool initialized = false;
    bool connected = false;
    uint32_t securityPassKey;
    
    // Internal callback classes - NO override keywords
    class RxCallbacks : public NimBLECharacteristicCallbacks {
    public:
        RxCallbacks(BLE_NUS_Service* service) : parent(service) {}
        void onWrite(NimBLECharacteristic* pCharacteristic);
        void onWrite(NimBLECharacteristic* pCharacteristic, const uint8_t* data, size_t len);
    private:
        BLE_NUS_Service* parent;
    };

    class ServerCallbacks : public NimBLEServerCallbacks {
    public:
        ServerCallbacks(BLE_NUS_Service* service) : parent(service) {}
        void onConnect(NimBLEServer* pServer);
        void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc);
        void onDisconnect(NimBLEServer* pServer);
        void onDisconnect(NimBLEServer* pServer, ble_gap_conn_desc* desc, int reason);
        uint32_t onPassKeyRequest();
        void onAuthenticationComplete(ble_gap_conn_desc* desc);
    private:
        BLE_NUS_Service* parent;
    };
    
    friend class RxCallbacks;
    friend class ServerCallbacks;
};

#endif