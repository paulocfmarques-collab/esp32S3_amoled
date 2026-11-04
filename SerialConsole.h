#pragma once
#include <Arduino.h>

// Leitura por linha, sem esperar pelo restante do comando.
class SerialConsole
{
  public:
    bool poll(Stream& port, String& command)
    {
        for (uint8_t consumed = 0; consumed < 64 && port.available(); ++consumed)
        {
            int value = port.read();
            if (value < 0)
                break;
            char c = static_cast<char>(value);
            if (c == '\r' || c == '\n')
            {
                if (discard_)
                {
                    port.println(
                        "[Serial] Comando descartado: limite de 255 bytes ou caractere invalido.");
                    discard_ = false;
                    length_ = 0;
                }
                else if (length_)
                {
                    buffer_[length_] = '\0';
                    command = buffer_;
                    length_ = 0;
                    return true;
                }
            }
            else if (!discard_)
            {
                if (c == '\b' || c == 127)
                {
                    if (length_)
                        --length_;
                }
                else if (c == '\0' || length_ == 255)
                {
                    discard_ = true;
                    length_ = 0;
                }
                else
                {
                    buffer_[length_++] = c;
                }
            }
        }
        return false;
    }

  private:
    char buffer_[256] = {};
    uint16_t length_ = 0;
    bool discard_ = false;
};
