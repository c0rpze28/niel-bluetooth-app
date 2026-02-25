#ifndef BLEUART_H
#define BLEUART_H

#include <NimBLEDevice.h>
#include <functional>
#include <string>

class BLEUart {
public:
    BLEUart(const std::string& deviceName);
    ~BLEUart();
    void begin();
    void send(const std::string& message);
    void onReceive(std::function<void(const std::string&)> callback);
    bool isConnected() const;  // added helper

private:
    std::string m_deviceName;
    NimBLEServer* m_pServer = nullptr;
    NimBLECharacteristic* m_pRxCharacteristic = nullptr;
    NimBLECharacteristic* m_pTxCharacteristic = nullptr;
    std::function<void(const std::string&)> m_receiveCallback;

    class ServerCallbacks;
    class RxCallbacks;
};

#endif