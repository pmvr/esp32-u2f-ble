#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>


#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "framing.h"

// Device Information Service
#define DEVICE_INFORMATION_SERVICE "0000180a-0000-1000-8000-00805f9b34fb" // 180A
#define MANUFACTURER_NAME_STRING   "00002a29-0000-1000-8000-00805f9b34fb" // 2A29
#define MODEL_NUMBER_STRING        "00002a24-0000-1000-8000-00805f9b34fb" // 2A24
#define FIRMWAREREVISIONSTRING     "00002a26-0000-1000-8000-00805f9b34fb" // 2A26

// U2F Service
#define U2F_SERVICE              "0000fffd-0000-1000-8000-00805f9b34fb" // FFFD
#define U2F_CONTROL_POINT        "f1d0fff1-deaa-ecee-b42f-c9ba7ed623bb"
#define U2F_STATUS               "f1d0fff2-deaa-ecee-b42f-c9ba7ed623bb"
#define U2F_CONTROL_POINT_LENGTH "f1d0fff3-deaa-ecee-b42f-c9ba7ed623bb"
#define U2F_SERVICE_REVISION     "00002a28-0000-1000-8000-00805f9b34fb" // 2A28
#define U2F_SERVICE_REVISION_BITFIELD "f1d0fff4-deaa-ecee-b42f-c9ba7ed623bb"
//#define CLIENT_CHARACTERISTIC_CONFIG  "00002902-0000-1000-8000-00805f9b34fb" // 2902

#define U2F_1_2 0x40

std::mutex mtx;
std::condition_variable cond_var;
std::mutex mtx_ka;
std::condition_variable cond_var_ka;

std::string response;
enum processingStatus status = Idle;

static uint32_t PIN = 111111;
static uint8_t U2fControlPointLength[] = { 0x00, 0x14 }; // 20 bytes
static uint8_t U2fServiceRevision[] = {U2F_1_2};  // U2F Version 1.2

static bool deviceConnected = false;
uint8_t buffer[100];
static BLECharacteristic *pU2fStatus;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class U2fControlPointCallback: public BLECharacteristicCallbacks {
  
  void onWrite(BLECharacteristic *pCharacteristic) {
    log_d("*** U2F Control Point ***");
    std::string value = pCharacteristic->getValue();
    if ( (status == Idle) && (value.length() > 0) ) {
        log_d("*********");
        log_d("New value: %02x", value[0]);
        log_d("*********");
        switch (update(value)) {
          case UPDATE_FRAMING:
            // status = Idle;
            break;
          case UPDATE_SUCCESS:
          case UPDATE_ERROR:
            std::unique_lock<std::mutex> lock(mtx);
            status = CmdReady;
            cond_var.notify_one();
            break;
        }
      }
    }
};

class U2fServiceRevisionCallback: public BLECharacteristicCallbacks {  
  void onWrite(BLECharacteristic *pCharacteristic) {
    log_d("*** U2F Service Revision ***");
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
        //U2fServiceRevision[0] = value[0];
        log_d("*********");
        log_d("New value: %.2x", value[0]);
        log_d("*********");
      }
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE-U2F!");

  log_v("Verbose");
  log_d("Debug");
  log_i("Info");
  log_w("Warning"); 
  log_e("Error");

  BLEDevice::init("ESPU2F");
  /*
   * Required in authentication process to provide displaying and/or input passkey or yes/no butttons confirmation
   */
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  // Device Information Service
  BLEService *pDevInfoService = pServer->createService(DEVICE_INFORMATION_SERVICE);
  // Characteristic Manufacturer Name String
  BLECharacteristic *pCharacteristic = pDevInfoService->createCharacteristic(
                                         MANUFACTURER_NAME_STRING,
                                         BLECharacteristic::PROPERTY_READ);
  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ);
  pCharacteristic->setValue("ESP32 U2f Demo");
  // Characteristic Model Number String
  pCharacteristic = pDevInfoService->createCharacteristic(
                      MODEL_NUMBER_STRING,
                      BLECharacteristic::PROPERTY_READ);
  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ);
  pCharacteristic->setValue("ESP32");
  // Characteristic Firmware Revision String
  pCharacteristic = pDevInfoService->createCharacteristic(
                      FIRMWAREREVISIONSTRING,
                      BLECharacteristic::PROPERTY_READ);
  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ);
  pCharacteristic->setValue("0.1");

  pDevInfoService->start();

  // U2F Service
  BLEService *pU2fService = pServer->createService(U2F_SERVICE);
  // Characteristic U2F Control Point
  BLECharacteristic *pU2fControlPoint = pU2fService->createCharacteristic(
                                          U2F_CONTROL_POINT,
                                          BLECharacteristic::PROPERTY_WRITE);
  pU2fControlPoint->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
  pU2fControlPoint->setCallbacks(new U2fControlPointCallback());
  // Characteristic U2F Status
  pU2fStatus = pU2fService->createCharacteristic(
                              U2F_STATUS,
                              BLECharacteristic::PROPERTY_READ   |
                              BLECharacteristic::PROPERTY_WRITE  |
                              BLECharacteristic::PROPERTY_NOTIFY);
  pU2fStatus->addDescriptor(new BLE2902());
  pU2fStatus->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  // Characteristic U2F Control Point Length
  pCharacteristic = pU2fService->createCharacteristic(
                      U2F_CONTROL_POINT_LENGTH,
                      BLECharacteristic::PROPERTY_READ);
  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
  pCharacteristic->setValue(U2fControlPointLength, sizeof(U2fControlPointLength));
  // Characteristic U2F Service Revision
  pCharacteristic = pU2fService->createCharacteristic(
                      U2F_SERVICE_REVISION,
                      BLECharacteristic::PROPERTY_READ);
  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
  pCharacteristic->setValue("Rev. 1.0");
  // Characteristic U2F Service Revision Bitfield
  pCharacteristic = pU2fService->createCharacteristic(
                      U2F_SERVICE_REVISION_BITFIELD,
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  pCharacteristic->setValue(U2fServiceRevision, sizeof(U2fServiceRevision));
  pCharacteristic->setCallbacks(new U2fServiceRevisionCallback());
  
  pU2fService->start();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();

  BLESecurity *pSecurity = new BLESecurity();
  // set static PIN
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &PIN, sizeof(uint32_t));
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);  // bonding with peer device after authentication
  pSecurity->setCapability(ESP_IO_CAP_OUT);  // set the IO capability to: Display Only
  pSecurity->setKeySize(16);
  uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
  esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));
  //uint8_t oob_support = ESP_BLE_OOB_DISABLE;
  //esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
}

void keepalive() {
  while (true) {
    auto const timeout= std::chrono::steady_clock::now()+std::chrono::milliseconds(100);
    std::unique_lock<std::mutex> lock(mtx_ka);
    if ( cond_var_ka.wait_until(lock, timeout) == std::cv_status::timeout) {
      log_d("*** keep alive"); 
    }
    else return;
    if (status == ResultReady) return;
  }
}

// simulates user presence
bool user_presence_check() {
  const char TUP_NEEDED[] = {0x82, 0x00, 0x01, 0x02};
  std::string ka = std::string(TUP_NEEDED, sizeof(TUP_NEEDED));
  for (uint8_t i=0; i<5; i++) {
      log_d("*** TUP_NEEDED: %08x", *((uint32_t*)TUP_NEEDED)); 
      pU2fStatus->setValue(ka);  // TUP_NEEDED: CMD HLEN LLEN DATA
      pU2fStatus->notify();
      delay(250);
  }
  return true;
}

void loop() {
  std::unique_lock<std::mutex> lock(mtx);
  while (status != CmdReady) {  // loop to avoid spurious wakeups
      cond_var.wait(lock);
  }
  status = IsProcessing;
  //std::thread thread_ka(keepalive);
  processCMD();
  status = ResultReady;
  /*std::unique_lock<std::mutex> lock_ka(mtx_ka);
  cond_var_ka.notify_one();
  thread_ka.join();
  */
  log_d("*** response length: %d", response.length());
  if (deviceConnected && (response.length() > 0)) {
    if (response.length() <= ATT_MTU) {
      pU2fStatus->setValue(response);
      pU2fStatus->notify();
    }
    else { // framing
      int16_t LEN = response.length();
      uint8_t framing_index = 0;
      int16_t start = 0;
      int16_t end = ATT_MTU;
      pU2fStatus->setValue(response.substr(start, end));
      pU2fStatus->notify();
      while (end < LEN) {
        start = end;
        end += ATT_MTU-1;
        end = min(LEN, end);
        std::string str_index = std::string(1, framing_index);
        pU2fStatus->setValue(str_index + response.substr(start, ATT_MTU-1));
        pU2fStatus->notify();
        framing_index += 1;
      }
    }
  }
  status = Idle;
}
