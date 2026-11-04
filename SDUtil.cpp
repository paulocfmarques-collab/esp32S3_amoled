#include "SDUtil.h"
#include "BoardConfig.h"
#include <esp_timer.h>
#include <esp_random.h>

bool SDUtil::validPath(const String& path) {
    return path.length() && path[0]=='/' && path.indexOf("..")<0 && path.indexOf('\\')<0;
}
bool SDUtil::begin() {
    Lock lock(*this);
    if (mounted_) return true;
    SD_MMC.end();
    if (!SD_MMC.setPins(BoardConfig::SdClock, BoardConfig::SdCommand, BoardConfig::SdData)) return false;
    // 1 bit, 20 MHz, sem formatar automaticamente o cartão.
    mounted_ = SD_MMC.begin("/sdcard", true, false, 20000) && SD_MMC.cardType()!=CARD_NONE;
    if (!mounted_) SD_MMC.end();
    metrics_.mounted = mounted_; metrics_.valid = false; metricsAt_ = 0;
    Serial.println(mounted_ ? "[SD] SDMMC 1 bit pronto" : "[SD] Cartao ausente ou falha de montagem");
    return mounted_;
}
String SDUtil::getCardType() {
    Lock lock(*this);
    if (!mounted_) return "ausente";
    switch (SD_MMC.cardType()) { case CARD_MMC:return "MMC";case CARD_SD:return "SDSC";case CARD_SDHC:return "SDHC/SDXC";default:return "desconhecido"; }
}
uint64_t SDUtil::getCardSizeBytes() { Lock lock(*this); return mounted_ ? SD_MMC.cardSize() : 0; }
uint64_t SDUtil::getUsedBytes(bool* valid) {
    Lock lock(*this);
    if (valid) *valid = mounted_;
    return mounted_ ? SD_MMC.usedBytes() : 0;
}
uint64_t SDUtil::getUsedBytesCooperative(bool* valid) { return getUsedBytes(valid); }
void SDUtil::processMetrics() {
    Lock lock(*this);
    if (!mounted_ || (metricsAt_ && millis()-metricsAt_<30000)) return;
    metrics_.mounted = true;
    metrics_.capacity = SD_MMC.cardSize(); metrics_.total = SD_MMC.totalBytes();
    metrics_.used = SD_MMC.usedBytes(); metrics_.valid = metrics_.total>0 && metrics_.used<=metrics_.total;
    for (size_t i=0;i<3;++i) {
        File file = SD_MMC.open(METRIC_LOG_PATHS[i], FILE_READ);
        metrics_.logValid[i] = bool(file) && !file.isDirectory();
        metrics_.logBytes[i] = metrics_.logValid[i] ? file.size() : 0;
        file.close();
    }
    metrics_.updatedUs = esp_timer_get_time(); metricsAt_ = millis();
}
SdMetricsSnapshot SDUtil::metricsSnapshot() { Lock lock(*this); return metrics_; }
bool SDUtil::exists(String path) { Lock lock(*this); return mounted_ && validPath(path) && SD_MMC.exists(path); }
bool SDUtil::write(const String& path, const String& text, bool append) {
    Lock lock(*this);
    bool ok = false;
    if (mounted_ && validPath(path)) {
        File file = SD_MMC.open(path, append ? FILE_APPEND : FILE_WRITE);
        if (file && !file.isDirectory()) {
            ok = file.write(reinterpret_cast<const uint8_t*>(text.c_str()), text.length())==text.length();
            file.flush(); ok = ok && !file.getWriteError();
        }
        file.close();
    }
    if (append) { if (ok) ++metrics_.appends; else ++metrics_.appendErrors; }
    if (ok) metricsAt_=0;
    return ok;
}
bool SDUtil::writeText(String path, String text) { return write(path,text,false); }
bool SDUtil::appendText(String path, String text) { return write(path,text,true); }
String SDUtil::readText(String path) { return readTail(path,16384); }
String SDUtil::readTail(String path, size_t maxBytes) {
    Lock lock(*this);
    if (!mounted_ || !validPath(path)) return "";
    File file = SD_MMC.open(path, FILE_READ);
    if (!file || file.isDirectory()) { file.close(); return ""; }
    const size_t limit = min(maxBytes, size_t(16384));
    const size_t length = min(size_t(file.size()), limit);
    if (!file.seek(file.size()-length)) { file.close(); return ""; }
    String content;
    if (!content.reserve(length+1)) { file.close(); return ""; }
    char buffer[257]; size_t left = length;
    while (left) {
        const size_t n = file.readBytes(buffer,min(left,size_t(256)));
        if (!n) break;
        buffer[n]=0; content+=buffer; left-=n;
    }
    file.close(); return content;
}
int SDUtil::readLogTail(const char* path, String& content, bool& truncated) {
    Lock lock(*this);
    content=""; truncated=false;
    if (!mounted_) { content="Cartao SD indisponivel"; return 503; }
    if (!path || !validPath(String(path))) return 400;
    if (!SD_MMC.exists(path)) return 404;
    File file=SD_MMC.open(path,FILE_READ);
    if (!file || file.isDirectory()) { file.close(); return 500; }
    const size_t expected=min(size_t(file.size()),size_t(16384));
    truncated=file.size()>expected; file.close();
    content=readTail(path,expected);
    return content.length()==expected ? 200 : 500;
}
bool SDUtil::removeFile(String path) {
    Lock lock(*this); bool ok=mounted_ && validPath(path) && SD_MMC.remove(path); if(ok)metricsAt_=0; return ok;
}
bool SDUtil::createDir(String path) { Lock lock(*this); return mounted_ && validPath(path) && SD_MMC.mkdir(path); }
bool SDUtil::removeDir(String path) { Lock lock(*this); return mounted_ && validPath(path) && path!="/" && SD_MMC.rmdir(path); }
bool SDUtil::renameFile(const String& from,const String& to) {
    Lock lock(*this); bool ok=mounted_ && validPath(from) && validPath(to) && SD_MMC.rename(from,to); if(ok)metricsAt_=0; return ok;
}
uint64_t SDUtil::getFileSize(String path) {
    Lock lock(*this);
    if (!mounted_ || !validPath(path)) return 0;
    File file=SD_MMC.open(path,FILE_READ); uint64_t size=file && !file.isDirectory()?file.size():0; file.close(); return size;
}
String SDUtil::listFiles(String path) {
    Lock lock(*this);
    if (!mounted_ || !validPath(path)) return "SD indisponivel\n";
    File dir=SD_MMC.open(path,FILE_READ);
    if (!dir || !dir.isDirectory()) { dir.close(); return "Diretorio indisponivel\n"; }
    String result;
    for (File file=dir.openNextFile();file;file=dir.openNextFile()) {
        result+=String(file.isDirectory()?"DIR  ":"FILE ")+file.name();
        if (!file.isDirectory()) result+="  "+String(file.size())+" bytes";
        result+='\n'; file.close();
        if (result.length()>8000) { result+="...\n";break; }
    }
    dir.close(); return result;
}
bool SDUtil::writeJson(String path,String content) { return writeText(path,content); }
String SDUtil::readJson(String path) { return readText(path); }
bool SDUtil::test() {
    Lock lock(*this);
    if (!mounted_) return false;
    String path;
    do { path="/.sd-test-"+String(esp_random(),HEX)+".tmp"; } while (exists(path));
    const String text="AMOLED SDMMC test\n";
    const bool wrote=writeText(path,text);
    const bool read=wrote && readText(path)==text;
    const bool removed=removeFile(path);
    return wrote && read && removed;
}
bool SDUtil::salvarCrash(const String& report,String& path,String& error) {
    Lock lock(*this); path=""; error="";
    if (!mounted_) { error="Cartao SD nao montado";return false; }
    if (!exists("/crashes") && !createDir("/crashes")) { error="Falha ao criar /crashes";return false; }
    do { path="/crashes/crash-"+String(esp_random(),HEX)+".txt"; } while (exists(path));
    if (!writeText(path,report)) { error="Falha ao gravar relatorio";path="";return false; }
    return true;
}
