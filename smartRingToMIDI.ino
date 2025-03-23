/**
 * A BLE client example that is rich in capabilities.
 * There is a lot new capabilities implemented.
 * author unknown
 * updated by chegewara
 * Further changes by Cybirbug
 * Big thanks to tanhok https://github.com/tahnok/colmi_r02_client
 * Big thanks to edgeimpulse for the better firmware and more documentation  https://github.com/edgeimpulse/example-data-collection-colmi-r02/blob/main/ring.py
 */

 /* UUID's of the service, characteristic that we want to read*/
 // BLE Service

//working esp32 S3 version 3.1.1

//firmware update!!!    https://github.com/edgeimpulse/example-data-collection-colmi-r02/blob/main/ring.py


#include "BLEDevice.h"



//MIDIusb midi;
 //#include "BLEScan.h"
String bleServerName = "R02_7AEB";
 // The remote service we wish to connect to.
BLEUUID serviceUUID("6E40FFF0-B5A3-F393-E0A9-E50E24DCCA9E");
BLEUUID subscribeTo("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");
BLEUUID writeTo(    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLERemoteCharacteristic* pWriteCharacteristic;
static BLEAdvertisedDevice* myDevice;

typedef union {//holds all the user mutable variables
    struct {
        uint8_t command;
        uint8_t data[14];
        uint8_t checksum;
    } __attribute__((packed));
    uint8_t raw[16];
} RingPacket;

typedef union {//holds all the user mutable variables
    struct {
        uint8_t command;
        uint8_t subtype;//1
        int16_t x;//1
        int16_t y;//1
        int16_t z;//1
        uint8_t data6;//1
        uint8_t data7;//1
        uint8_t data8;//1
        uint8_t data9;//1
        uint8_t data10;//1
        uint8_t data11;//1
       // uint8_t data12;//1
       // uint8_t data13;//1
       // uint8_t data14;//1
        uint8_t checksum;
    } __attribute__((packed));
    uint8_t raw[16];
} RingGyroPacket;

int xTracker = 0;
int yTracker = 0;
int zTracker = 0;

int xOut = 0;
int yOut = 0;
int zOut = 0;

int xLastOut = 0;
int yLastOut = 0;
int zLastOut = 0;

uint outputTimer;

int makeChecksum(RingPacket packet) {
    //add it all together, modulus 255 which can be simplified to &
    int sum = 0;

    for (int i = 0; i < sizeof(packet) - 1; i++) {
        sum += packet.raw[i];
    }
    return sum & 255;
}

void sendRequest(int command) {
    RingPacket thisPacket;
    thisPacket.command = command;
    thisPacket.checksum = makeChecksum(thisPacket);
    pWriteCharacteristic->writeValue(thisPacket.raw, 16, true);
}

void sendReboot(int command) {
    RingPacket thisPacket;
    thisPacket.command = 8;
    thisPacket.checksum = makeChecksum(thisPacket);
    pWriteCharacteristic->writeValue(thisPacket.raw, 16, true);
}


void sendFastDataRequest() {
    RingPacket thisPacket;
    thisPacket.command = 0x02;
    //thisPacket.command = 0xA1;
    //thisPacket.data[0] = 0x04;//these give all data!
    thisPacket.data[0] = 0x04;
    thisPacket.checksum = makeChecksum(thisPacket);
    pWriteCharacteristic->writeValue(thisPacket.raw, 16, true);
}
void stopFastDataRequest() {
    RingPacket thisPacket;
    thisPacket.command = 0x01;
    thisPacket.data[0] = 0x02;
    thisPacket.checksum = makeChecksum(thisPacket);
    pWriteCharacteristic->writeValue(thisPacket.raw, 16, true);
}

static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {

    RingGyroPacket gyroData;

    gyroData.command = pData[0];
    gyroData.subtype = pData[1];
    if (gyroData.subtype == 0x03 && gyroData.command == 0xA1) {
        //12 bit integers must be costructed
        gyroData.x = ((pData[6] << 4) | (pData[7] & 0xF)) & 0xFFF;
        gyroData.y = ((pData[2] << 4) | (pData[3] & 0xF)) & 0xFFF;
        gyroData.z = ((pData[4] << 4) | (pData[5] & 0xF)) & 0xFFF;

        //Serial.print("x ");
        //Serial.print(gyroData.x);

        //Serial.print("  y ");
        //Serial.print(gyroData.y);

        //Serial.print("  z ");
        //Serial.println(gyroData.z);

        //byte notex = gyroData.x / 32;
        //byte notey = gyroData.y / 32;
        //byte notez = gyroData.z / 32;

        xTracker = (gyroData.x) / 32;
        yTracker = (gyroData.y) / 32;
        zTracker = (gyroData.z) / 32;


        if (xTracker > 127)xTracker = 127;
        if (xTracker < 0)xTracker = 0;
        if (yTracker > 127)yTracker = 127;
        if (yTracker < 0)yTracker = 0;
        if (zTracker > 127)zTracker = 127;
        if (zTracker < 0)zTracker = 0;

        //sendMidiNote(1, xTracker);
        //sendMidiNote(2, yTracker);
        //sendMidiNote(3, zTracker);
    }
    
}

class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {}

    void onDisconnect(BLEClient* pclient) {
        connected = false;
        Serial.println("onDisconnect");
        doConnect = true;
    }
};

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
    Serial.println(myDevice->getName().c_str());

    BLEClient* pClient = BLEDevice::createClient();
    Serial.println(" - Created client ");
    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice->getAddress());  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.print(" - Connected to server");

    pClient->setMTU(64);  //set client to request maximum MTU from server (default is 23 otherwise)// was 517

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    BLERemoteService* pWriteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        Serial.print("Failed to find our service UUID: ");
        Serial.println(serviceUUID.toString().c_str());
        pClient->disconnect();
        doConnect = true;
        return false;
    }
    Serial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(subscribeTo);
    if (pRemoteCharacteristic == nullptr) {
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(subscribeTo.toString().c_str());
        pClient->disconnect();
        doConnect = true;
        return false;
    }
    Serial.println(" - Found our characteristic subscribeto");

    pWriteCharacteristic = pWriteService->getCharacteristic(writeTo);
    if (pRemoteCharacteristic == nullptr) {
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(writeTo.toString().c_str());
        pClient->disconnect();
        doConnect = true;
        return false;
    }
    Serial.println(" - Found our characteristic writeto");

    // Read the value of the characteristic.
    if (pRemoteCharacteristic->canRead()) {
        String value = pRemoteCharacteristic->readValue().c_str();
        Serial.print("The characteristic value was: ");
        Serial.println(value.c_str());
    }

    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }

    connected = true;
    //stopFastDataRequest();
    //delay(1000);
    //stopFastDataRequest();
    //delay(1000);
    sendFastDataRequest();
    return true;
}
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    /**
     * Called for each advertising BLE server.
     */
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        Serial.print("BLE Advertised Device found: ");
        Serial.println(advertisedDevice.toString().c_str());
        if (advertisedDevice.getName().length() != 8) return;
        bool same = true;
        for (int i = 0; i < 8; i++) {
            if (advertisedDevice.getName().c_str()[i] != bleServerName.c_str()[i]) same = false;
        }

        // We have found a device, let us now see if it contains the service we are looking for.
        if (same) {
            Serial.println("SAME");
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = false;

        }  // Found our server
        else {
        }
    }  // onResult
};  // MyAdvertisedDeviceCallbacks

void sendMidiNote(byte note, byte value) {

    Serial.write(0x91);
    Serial.write(note);
    Serial.write(value);
    //Serial.println(0x91);
    //Serial.println(note);
    //Serial.println(value);
}

void setup() {
    Serial.begin(115200);
    //Serial.begin(31250);
    Serial.println("Starting Arduino BLE Client application...");
    BLEDevice::init("");

    // Retrieve a Scanner and set the callback we want to use to be informed when we
    // have detected a new device.  Specify that we want active scanning and start the
    // scan to run for 20 seconds.
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(20, false);
    pBLEScan->clearResults();

    outputTimer = millis();
}  // End of setup.

// This is the Arduino main loop function.
void loop() {
    if (doConnect == true) {
        if (connectToServer()) {
           // Serial.println("Conncected");
        }
        else {
            //fail, trying again
        }
        doConnect = false;
    }


    if (connected) {
//don't worry be happy
    }
    else if (doScan) {
        BLEScan* pBLEScan = BLEDevice::getScan();
        pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
        pBLEScan->setInterval(1349);
        pBLEScan->setWindow(449);
        pBLEScan->setActiveScan(true);
        pBLEScan->start(30, false);
        pBLEScan->clearResults();
    }

    int time = millis();
    if (time - outputTimer > 50) {
        outputTimer = time;

        int divisor = 3;

        if (xTracker > xOut) xOut += (xTracker - xOut) / divisor;
        if (xTracker < xOut) xOut-= (xOut - xTracker) / divisor;
        if (yTracker > yOut) yOut += (yTracker - yOut) / divisor;
        if (yTracker < yOut) yOut -= (yOut - yTracker) / divisor;
        if (zTracker > zOut) zOut += (zTracker - zOut) / divisor;
        if (zTracker < zOut) zOut -= (zOut - zTracker) / divisor;

        if (xOut > 127)xOut = 127;
        if (xOut < 0)xOut = 0;
        if (yOut > 127)yOut = 127;
        if (yOut < 0)yOut = 0;
        if (zOut > 127)zOut = 127;
        if (zOut < 0)zOut = 0;

        if (xLastOut != xOut) {
            sendMidiNote(1, xOut);
            xLastOut = xOut;
        }
        if (yLastOut != yOut) {
            sendMidiNote(2, yOut);
            yLastOut = yOut;
        }
        if (zLastOut != zOut) {
            sendMidiNote(3, zOut);
            zLastOut = zOut;
        }
    }
}  // End of loop
