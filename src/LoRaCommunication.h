#pragma once

#include <Arduino.h>

#ifdef ENABLE_LORA
#include <vector>
#endif

class LoRaCommunication {
public:
    void setup();
    void loop();
    bool isTransmissionActive() const;
    bool isRadioPaused() const;

private:
    bool transmissionActive = false;
    bool radioPaused = false;
    uint8_t pausedMode = 0;

#ifdef ENABLE_LORA
    void handleRequest(const String &request);
    void preparePacketsForRequest(const String &requestType);
    void appendListData(const String &requestType);
    void sendImmediate(const String &message);
    void startTransmission();
    void sendNextPacket();
    void finishTransmission(bool success, const String &reason);
    void pauseCaptures();
    void resumeCaptures();
    String sanitize(const String &value) const;

    bool loraReady = false;
    unsigned long lastPacketSentTime = 0;
    unsigned long lastActivityTime = 0;
    size_t currentPacketIndex = 0;
    String activeRequest;
    std::vector<String> packetsToSend;
#endif
};

extern LoRaCommunication LoRaComm;
