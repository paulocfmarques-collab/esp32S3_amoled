#pragma once
#include <Arduino.h>

class SDUtil;

class CrashDiagnostics
{
  public:
    static String lerArquivarEApagar(SDUtil& sd, const String& motivoReset);
};
