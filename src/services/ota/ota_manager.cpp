#include "services/ota/ota_manager.h"

#include <LittleFS.h>
#include <Update.h>

namespace OtaManager {
namespace {
WebServer* otaServer = nullptr;
AuthenticationCheck authenticationCheck = nullptr;
bool updateStarted = false;
bool updateFailed = false;
bool updateFinished = false;
bool uploadRejected = false;
bool filesystemUnmounted = false;
int activeUpdateCommand = U_FLASH;
String updateError;

bool isAuthorized()
{
    return authenticationCheck != nullptr && authenticationCheck();
}

void handleUploadStream(int updateCommand)
{
    if (!isAuthorized())
    {
        uploadRejected = true;
        return;
    }

    HTTPUpload& upload = otaServer->upload();
    switch (upload.status)
    {
    case UPLOAD_FILE_START:
    {
        updateStarted = false;
        updateFailed = false;
        updateFinished = false;
        uploadRejected = false;
        activeUpdateCommand = updateCommand;
        updateError = "";

        String filename = upload.filename;
        filename.toLowerCase();
        if (!filename.endsWith(".bin"))
        {
            updateFailed = true;
            updateError = "Chi chap nhan file co duoi .bin.";
            return;
        }
        if (activeUpdateCommand == U_FLASHFS && !filename.endsWith("littlefs.bin"))
        {
            updateFailed = true;
            updateError = "Hay chon file littlefs.bin duoc tao tu PlatformIO.";
            return;
        }

        if (activeUpdateCommand == U_FLASHFS)
        {
            LittleFS.end();
            filesystemUnmounted = true;
        }

        if (!Update.begin(UPDATE_SIZE_UNKNOWN, activeUpdateCommand))
        {
            updateFailed = true;
            updateError = Update.errorString();
            return;
        }
        updateStarted = true;
        Serial.printf("OTA: bat dau nap %s\n", upload.filename.c_str());
        break;
    }

    case UPLOAD_FILE_WRITE:
        if (updateStarted && !updateFailed)
        {
            const size_t written = Update.write(upload.buf, upload.currentSize);
            if (written != upload.currentSize)
            {
                updateFailed = true;
                updateError = Update.errorString();
                Update.abort();
            }
        }
        break;

    case UPLOAD_FILE_END:
        if (updateStarted && !updateFailed)
        {
            if (Update.end(true))
            {
                updateFinished = true;
                Serial.printf("OTA: da ghi %u byte\n", static_cast<unsigned>(upload.totalSize));
            }
            else
            {
                updateFailed = true;
                updateError = Update.errorString();
            }
        }
        break;

    case UPLOAD_FILE_ABORTED:
        if (updateStarted)
        {
            Update.abort();
        }
        updateStarted = false;
        updateFailed = true;
        updateError = "Qua trinh upload bi huy.";
        break;

    default:
        break;
    }
}

void finishUpload(int updateCommand)
{
    if (!isAuthorized() || uploadRejected)
    {
        if (filesystemUnmounted)
        {
            LittleFS.begin(false);
            filesystemUnmounted = false;
        }
        otaServer->send(401, "text/plain; charset=utf-8", "Vui long dang nhap de cap nhat firmware.");
        return;
    }

    if (!updateFinished || updateFailed)
    {
        String message = "Cap nhat firmware that bai.";
        if (updateError.length())
        {
            message += " ";
            message += updateError;
        }
        if (filesystemUnmounted)
        {
            LittleFS.begin(false);
            filesystemUnmounted = false;
        }
        otaServer->send(400, "text/plain; charset=utf-8", message);
        return;
    }

    const char* successMessage = updateCommand == U_FLASHFS
                                     ? "Filesystem da cap nhat. ESP32 dang khoi dong lai."
                                     : "Firmware da cap nhat. ESP32 dang khoi dong lai.";
    otaServer->send(200, "text/plain; charset=utf-8", successMessage);
    Serial.println(successMessage);
    delay(1000);
    ESP.restart();
}

void handleFirmwareStream()
{
    handleUploadStream(U_FLASH);
}

void handleFilesystemStream()
{
    handleUploadStream(U_FLASHFS);
}

void handleFirmwareResult()
{
    finishUpload(U_FLASH);
}

void handleFilesystemResult()
{
    finishUpload(U_FLASHFS);
}
} // namespace

void registerRoutes(WebServer& server, AuthenticationCheck isAuthenticated)
{
    otaServer = &server;
    authenticationCheck = isAuthenticated;
    server.on("/ota/update", HTTP_POST, handleFirmwareResult, handleFirmwareStream);
    server.on("/ota/filesystem", HTTP_POST, handleFilesystemResult, handleFilesystemStream);
}

} // namespace OtaManager
