# Bundled display fonts

The FreeSans bitmap headers were copied from the locally installed Adafruit GFX font collection. The only edit to the headers is replacing the Adafruit_GFX include with Arduino_GFX_Library so the existing renderer supplies the GFXfont types. Glyph data was not changed. Font arrays are included only by UiPainter.cpp.

- Collection source: https://github.com/adafruit/Adafruit-GFX-Library/tree/master/Fonts
- Collection license notice: ADAFRUIT-LICENSE.txt (preserved).
- Typeface origin: GNU FreeFont / FreeSans, https://www.gnu.org/software/freefont/
- GNU font terms: GPL-3.0-or-later with Font-exception-2.0. See GPL-3.0.txt and FONT-EXCEPTION.txt and https://www.gnu.org/s/freefont/license.html.
- This notice does not assign a license to the project's own firmware. Retain upstream notices and applicable font licensing when distributing the bundled font software.

Files: FreeSans9pt7b.h, FreeSans12pt7b.h, FreeSansBold18pt7b.h, FreeSansBold24pt7b.h.
