#include "services/page_manager/page_manager.h"

#include <LittleFS.h>

namespace PageManager {
namespace {
WebServer* pageServer = nullptr;
AuthenticationCheck authenticationCheck = nullptr;
File temporaryFile;
String targetPath;
String temporaryPath;
String uploadError;
bool uploadRejected = false;
bool uploadFailed = false;
bool uploadFinished = false;

bool isAuthorized()
{
    return authenticationCheck != nullptr && authenticationCheck();
}

void removeTemporaryFile()
{
    if (temporaryFile)
    {
        temporaryFile.close();
    }
    if (temporaryPath.length() && LittleFS.exists(temporaryPath))
    {
        LittleFS.remove(temporaryPath);
    }
}

void handlePageUpload()
{
    if (!isAuthorized())
    {
        uploadRejected = true;
        return;
    }

    HTTPUpload& upload = pageServer->upload();
    switch (upload.status)
    {
    case UPLOAD_FILE_START:
    {
        uploadRejected = false;
        uploadFailed = false;
        uploadFinished = false;
        uploadError = "";
        targetPath = "";
        temporaryPath = "";

        String filename = upload.filename;
        const int slash = filename.lastIndexOf('/');
        const int backslash = filename.lastIndexOf('\\');
        const int separator = slash > backslash ? slash : backslash;
        if (separator >= 0)
        {
            filename = filename.substring(separator + 1);
        }
        filename.toLowerCase();

        if (filename == "login.html")
        {
            targetPath = "/pages/login.html";
        }
        else if (filename == "dashboard.html")
        {
            targetPath = "/pages/dashboard.html";
        }
        else
        {
            uploadFailed = true;
            uploadError = "Chi nhan login.html hoac dashboard.html.";
            return;
        }

        temporaryPath = targetPath + ".upload";
        if (LittleFS.exists(temporaryPath))
        {
            LittleFS.remove(temporaryPath);
        }
        temporaryFile = LittleFS.open(temporaryPath, FILE_WRITE);
        if (!temporaryFile)
        {
            uploadFailed = true;
            uploadError = "Khong tao duoc file tam trong LittleFS.";
        }
        break;
    }

    case UPLOAD_FILE_WRITE:
        if (temporaryFile && !uploadFailed)
        {
            const size_t written = temporaryFile.write(upload.buf, upload.currentSize);
            if (written != upload.currentSize)
            {
                uploadFailed = true;
                uploadError = "Ghi file HTML khong day du.";
                temporaryFile.close();
            }
        }
        break;

    case UPLOAD_FILE_END:
        if (temporaryFile && !uploadFailed)
        {
            temporaryFile.close();
            const String backupPath = targetPath + ".backup";
            if (LittleFS.exists(backupPath))
            {
                LittleFS.remove(backupPath);
            }

            const bool hadOriginal = LittleFS.exists(targetPath);
            if (hadOriginal && !LittleFS.rename(targetPath, backupPath))
            {
                uploadFailed = true;
                uploadError = "Khong the backup trang HTML hien tai.";
            }
            else if (!LittleFS.rename(temporaryPath, targetPath))
            {
                uploadFailed = true;
                uploadError = "Khong the thay the trang HTML.";
                if (hadOriginal && LittleFS.exists(backupPath))
                {
                    LittleFS.rename(backupPath, targetPath);
                }
            }
            else
            {
                if (hadOriginal && LittleFS.exists(backupPath))
                {
                    LittleFS.remove(backupPath);
                }
                uploadFinished = true;
                Serial.print("HTML page uploaded: ");
                Serial.println(targetPath);
            }
        }
        removeTemporaryFile();
        break;

    case UPLOAD_FILE_ABORTED:
        uploadFailed = true;
        uploadError = "Qua trinh upload HTML bi huy.";
        removeTemporaryFile();
        break;

    default:
        break;
    }
}

void handlePageUploadResult()
{
    if (!isAuthorized() || uploadRejected)
    {
        removeTemporaryFile();
        pageServer->send(401, "text/plain; charset=utf-8", "Vui long dang nhap de tai len HTML.");
        return;
    }

    if (!uploadFinished || uploadFailed)
    {
        removeTemporaryFile();
        String message = "Tai len trang HTML that bai.";
        if (uploadError.length())
        {
            message += " ";
            message += uploadError;
        }
        pageServer->send(400, "text/plain; charset=utf-8", message);
        return;
    }

    pageServer->send(200, "text/plain; charset=utf-8",
                     "Da cap nhat HTML. Tai lai trang de xem thay doi.");
}
} // namespace

void registerRoutes(WebServer& server, AuthenticationCheck isAuthenticated)
{
    pageServer = &server;
    authenticationCheck = isAuthenticated;
    server.on("/pages/upload", HTTP_POST, handlePageUploadResult, handlePageUpload);
}

} // namespace PageManager
