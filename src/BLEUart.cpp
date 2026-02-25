#include "BLEUart.h"

#define NUS_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHAR_UUID        "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_CHAR_UUID        "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class BLEUart::ServerCallbacks : public NimBLEServerCallbacks {
public:
    explicit ServerCallbacks(BLEUart* uart) : m_uart(uart) {}

    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        Serial.println("BLE client connected");
        NimBLEDevice::getAdvertising()->stop();
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        Serial.printf("BLE client disconnected, reason: %d\n", reason);
        delay(500);
        NimBLEDevice::startAdvertising();
        Serial.println("BLE advertising restarted");
    }

private:
    BLEUart* m_uart;
};

class BLEUart::RxCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit RxCallbacks(BLEUart* uart) : m_uart(uart) {}

    // Correct signature for NimBLE-Arduino 2.x
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        std::string rxValue = pCharacteristic->getValue();
        if (!rxValue.empty() && m_uart->m_receiveCallback) {
            m_uart->m_receiveCallback(rxValue);
        }
    }

private:
    BLEUart* m_uart;
};

BLEUart::BLEUart(const std::string& deviceName) : m_deviceName(deviceName) {}

BLEUart::~BLEUart() {}

void BLEUart::begin() {
    NimBLEDevice::init(m_deviceName);

    // Create server with minimal callbacks
    m_pServer = NimBLEDevice::createServer();
    m_pServer->setCallbacks(new ServerCallbacks(this));

    NimBLEService* pService = m_pServer->createService(NUS_SERVICE_UUID);

    m_pRxCharacteristic = pService->createCharacteristic(
        NUS_RX_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    m_pRxCharacteristic->setCallbacks(new RxCallbacks(this));

    m_pTxCharacteristic = pService->createCharacteristic(
        NUS_TX_CHAR_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // CCCD descriptor for notifications
    NimBLEDescriptor* cccd = new NimBLEDescriptor("2902", 0, 2, m_pTxCharacteristic);
    uint8_t cccd_value[2] = {0x00, 0x00};
    cccd->setValue(cccd_value, 2);
    m_pTxCharacteristic->addDescriptor(cccd);

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(NUS_SERVICE_UUID);
    NimBLEDevice::startAdvertising();

    Serial.println("BLE UART server started and advertising");
}

void BLEUart::send(const std::string& message) {
    if (m_pTxCharacteristic) {
        m_pTxCharacteristic->setValue(message);
        m_pTxCharacteristic->notify();
    }
}

void BLEUart::onReceive(std::function<void(const std::string&)> callback) {
    m_receiveCallback = callback;
}

bool BLEUart::isConnected() const {
    return m_pServer && m_pServer->getConnectedCount() > 0;
}