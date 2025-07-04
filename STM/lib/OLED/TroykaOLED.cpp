#include "TroykaOLED.h"
#include <cstring>
#include <cstdio>

TroykaOLED::TroykaOLED(uint8_t address, uint8_t width, uint8_t height)
    : _i2c(nullptr), _i2cAddress(address), _width(width), _height(height),
      _last{0, 0}, _codingName(Encoding::UTF8), _stateInvert(false),
      _stateAutoUpdate(true), _imageColor(WHITE), _screenBuffer{},
      _font{nullptr, nullptr, 0, 0, false, WHITE, false},
      _changedColumns{-1, -1} {
    memset(_screenBuffer.asBytes, 0, sizeof(_screenBuffer.asBytes));
}

void TroykaOLED::testFillScreen(uint8_t color) {
    uint8_t fillByte = (color == WHITE) ? 0xFF : 0x00;
    for (int i = 0; i < sizeof(_screenBuffer.asBytes); ++i) {
        _screenBuffer.asBytes[i] = fillByte;
    }
    _markChangedColumns(0, _width - 1);
    updateAll();
}

void TroykaOLED::testDrawCenterDot() {
    int16_t x = _width / 2;
    int16_t y = _height / 2;

    // Вычисляем страницу и бит для Y
    uint8_t page = y / 8;
    uint8_t bit = y % 8;

    // Индекс байта в массиве
    uint16_t index = x * (_height / 8) + page;

    // Устанавливаем один бит
    _screenBuffer.asBytes[index] |= (1 << bit);

    _markChangedColumns(x, x);
    updateAll();
}



void TroykaOLED::begin(I2C* i2c) {
    _i2c = i2c;

    _sendCommand(SSD1306_DISPLAY_OFF);
    _sendCommand(SSD1306_SET_DISPLAY_CLOCK, 0x80);
    _sendCommand(SSD1306_SET_MULTIPLEX_RATIO, 0x3F);
    _sendCommand(SSD1306_SET_DISPLAY_OFFSET, 0);
    _sendCommand(SSD1306_SET_START_LINE | 0);
    _sendCommand(SSD1306_CHARGE_DCDC_PUMP, 0x14);
    _sendCommand(SSD1306_ADDR_MODE, 0x01); // вертикальный режим
    _sendCommand(SSD1306_SET_REMAP_L_TO_R);
    _sendCommand(SSD1306_SET_REMAP_T_TO_D);
    _sendCommand(SSD1306_SET_COM_PINS, 0x12);
    _sendCommand(SSD1306_SET_CONTRAST, 0xFF);
    _sendCommand(SSD1306_SET_PRECHARGE_PERIOD, 0xF1);
    _sendCommand(SSD1306_SET_VCOM_DESELECT, 0x40);
    _sendCommand(SSD1306_RAM_ON);
    _sendCommand(SSD1306_INVERT_OFF);
    _sendCommand(SSD1306_DISPLAY_ON); // важно!
    
    clearDisplay();
}

void TroykaOLED::_interpretParameters(int16_t x, int16_t y, int16_t w, int16_t h) {
    // X координата
    switch (x) {
        case OLED_LEFT:   _last.x = 0; break;
        case OLED_CENTER: _last.x = (_width - w) / 2; break;
        case OLED_RIGHT:  _last.x = _width - w; break;
        default:          _last.x = x; break;
    }

    // Y координата
    switch (y) {
        case OLED_TOP:    _last.y = 0; break;
        case OLED_CENTER: _last.y = (_height - h) / 2; break;
        case OLED_BOTTOM: _last.y = _height - h; break;
        default:          _last.y = y; break;
    }
}

void TroykaOLED::update() {
    if (_changedColumns.right > -1 || _changedColumns.left < _width + 2) {
        _changedColumns.left = (_changedColumns.left < 0) ? 0 : _changedColumns.left;
        _changedColumns.right = (_changedColumns.right > _width - 1) ? _width - 1 : _changedColumns.right;
        _sendColumns(_changedColumns.left, _changedColumns.right);
        _changedColumns.left = _width + 2;
        _changedColumns.right = -1;
    }
}

void TroykaOLED::updateAll() {
    _sendColumns(0, _width - 1);
    _changedColumns.left = _width + 2;
    _changedColumns.right = -1;
}

void TroykaOLED::autoUpdate(bool stateAutoUpdate) {
    _stateAutoUpdate = stateAutoUpdate;
}

void TroykaOLED::setBrightness(uint8_t brightness) {
    _sendCommand(SSD1306_SET_CONTRAST, brightness);
}

void TroykaOLED::clearDisplay() {
    memset(_screenBuffer.asBytes, 0, sizeof(_screenBuffer.asBytes));
    _markChangedColumns(0, _width - 1);
    if (_stateAutoUpdate) update();
}

void TroykaOLED::invertDisplay(bool stateInvert) {
    _sendCommand(stateInvert ? SSD1306_INVERT_ON : SSD1306_INVERT_OFF);
}

void TroykaOLED::invertText(bool stateInvertText) {
    _font.invert = stateInvertText;
}

void TroykaOLED::textColor(uint8_t color) {
    _font.color = color;
}

void TroykaOLED::imageColor(uint8_t color) {
    _imageColor = color;
}

void TroykaOLED::setFont(const uint8_t* fontData) {
    _font.data = fontData;
    _font.width = fontData[0];
    _font.height = fontData[1]; 

    char type = fontData[2];
    if (_font.height % 8 != 0) {
        _font.setFont = false;
        return;
    }

    if (type == 'D') {
        _font.remap = digitsFontRemap;
        _font.setFont = true;
    } else if (type == 'A') {
        _font.remap = alphabetFontRemap;
        _font.setFont = true;
    } else {
        _font.setFont = false;
    }

    _font.color = WHITE;
}

void TroykaOLED::setCoding(Encoding codingName) {
    _codingName = codingName;
}

void TroykaOLED::setCursor(int16_t x, int16_t y) {
    _last.x = x;
    _last.y = y;
}

void TroykaOLED::print(char character, int16_t x, int16_t y) {
    _print(character, x, y);
}

void TroykaOLED::print(const char* line, int16_t x, int16_t y) {
    char data[128];
    strncpy(data, line, sizeof(data) - 1);
    data[sizeof(data) - 1] = '\0';
    _print(_encodeToCP866((uint8_t*)data), x, y);
}

void TroykaOLED::print(int8_t number, int16_t x, int16_t y, uint8_t base) {
    char buffer[16];
    sprintf(buffer, "%d", number);
    print(buffer, x, y);
}

void TroykaOLED::print(uint8_t number, int16_t x, int16_t y, uint8_t base) {
    char buffer[16];
    sprintf(buffer, "%u", number);
    print(buffer, x, y);
}

void TroykaOLED::print(int16_t number, int16_t x, int16_t y, uint8_t base) {
    char buffer[16];
    sprintf(buffer, "%d", number);
    print(buffer, x, y);
}

void TroykaOLED::print(uint16_t number, int16_t x, int16_t y, uint8_t base) {
    char buffer[16];
    sprintf(buffer, "%u", number);
    print(buffer, x, y);
}

void TroykaOLED::print(int32_t number, int16_t x, int16_t y, uint8_t base) {
    char buffer[32];
    sprintf(buffer, "%ld", number);
    print(buffer, x, y);
}

void TroykaOLED::print(uint32_t number, int16_t x, int16_t y, uint8_t base) {
    char buffer[32];
    sprintf(buffer, "%lu", number);
    print(buffer, x, y);
}

void TroykaOLED::print(double number, int16_t x, int16_t y, uint8_t digits) {
    char buffer[32];
    char format[10];
    snprintf(format, sizeof(format), "%%.%df", digits);
    snprintf(buffer, sizeof(buffer), format, number);
    print(buffer, x, y);
}

void TroykaOLED::drawPixel(int16_t x, int16_t y, uint8_t color) {
    _drawPixel(x, y, color);
    if (_stateAutoUpdate) update();
    _last.x = x;
    _last.y = y;
}

void TroykaOLED::drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) {
    _drawLine(x1, y1, x2, y2, color);
    if (_stateAutoUpdate) update();
    _last.x = x2;
    _last.y = y2;
}

void TroykaOLED::drawLine(int16_t x2, int16_t y2, uint8_t color) {
    drawLine(_last.x, _last.y, x2, y2, color);
}

void TroykaOLED::drawRect(int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool fill, uint8_t color) {
    // Реализация в следующих версиях
}

void TroykaOLED::drawCircle(int16_t x, int16_t y, uint8_t r, bool fill, uint8_t color) {
    // Реализация в следующих версиях
}

void TroykaOLED::drawImage(const uint8_t* image, int16_t x, int16_t y, uint8_t mem) {
    int16_t w = getImageWidth(image, mem);
    int16_t h = getImageHeight(image, mem);
    _interpretParameters(x, y, w, h);

    for (int16_t i = 0; i < w; i++) {
        if (_last.x + i >= 0 && _last.x + i < _width) {
            uint64_t col = 0;
            for (int16_t j = 0; j < (h >> 3); j++) {
                col |= (uint64_t)(image[j * w + i + 2]) << (j * 8);
            }
            _stamp(_last.x + i, _last.y, col, _imageColor);
        }
    }
    _markChangedColumns(_last.x, _last.x + w);
    _last.x += w;
    if (_stateAutoUpdate) update();
}

uint8_t TroykaOLED::getPixel(int16_t x, int16_t y) {
    if (x >= 0 && x < _width && y >= 0 && y < _height)
        return (_screenBuffer.column[x] >> y) & 1;
    return BLACK;
}

uint8_t TroykaOLED::getImageWidth(const uint8_t* image, uint8_t mem) {
    return image[0];
}

uint8_t TroykaOLED::getImageHeight(const uint8_t* image, uint8_t mem) {
    return image[1];
}

uint8_t TroykaOLED::_charToGlyph(char c) const {
    if (_font.remap != nullptr) {
        return _font.remap[(uint8_t)c];
    }
    return (uint8_t)c;
}
void TroykaOLED::_print(char c, int16_t x, int16_t y) {
    uint8_t index = _charToGlyph(c);

    //printf("Character '%c' -> CP866 %02X -> Glyph index %02X\n", c, (uint8_t)c, index);

    for (int i = 0; i < _font.width; ++i) {
        for (int j = 0; j < (_font.height >> 3); ++j) {
            uint8_t byte = _font.data[3 + index * (_font.height >> 3) * _font.width + j * _font.width + i];

            for (int b = 0; b < 8; b++) {
                if ((byte >> b) & 1) {
                    drawPixel(x + i, y + j * 8 + b, _font.color);
                }
            }
        }
    }

    _markChangedColumns(x, x + _font.width);
}

void TroykaOLED::_print(char* s, int16_t x, int16_t y) {
    _interpretParameters(x, y, strlen(s) * _font.width, _font.height);
    int16_t currentX = _last.x;
    int16_t currentY = _last.y;

    for (uint16_t i = 0; i < strlen(s); i++) {
        if (currentX >= 0 && currentX < _width) {
            _print(s[i], currentX, currentY);
        }
        currentX += _font.width;
    }

    if (_stateAutoUpdate) update();
}

char TroykaOLED::_itoa(uint8_t number) {
    return number < 10 ? '0' + number : 'A' + (number - 10);
}

char* TroykaOLED::_encodeToCP866(uint8_t* strIn) {
    switch (_codingName) {
        case Encoding::CP866:
            // если кодировка уже CP866, возвращаем строку без изменений
            return reinterpret_cast<char*>(strIn);

        case Encoding::UTF8:
            // здесь будет обработка UTF-8 → CP866
            break;

        default:
            break;
    }

    static char buffer[128];
    int j = 0;

    for (int i = 0; strIn[i] && j < sizeof(buffer) - 1;) {
        uint8_t c = strIn[i];

        if (c == 0xD0 && i + 1 < sizeof(buffer)) {
            uint8_t next = strIn[i + 1];
            if (next >= 0x90 && next <= 0xBF) {
                // А–Я
                buffer[j++] = next - 0x10;
                i += 2;
            } else if (next == 0x81) {
                // 'Ё'
                buffer[j++] = 0xF1;
                i += 2;
            }
        } else if (c == 0xD1 && i + 1 < sizeof(buffer)) {
            uint8_t next = strIn[i + 1];
            if (next >= 0x80 && next <= 0x8F) {
                // а–п
                buffer[j++] = next + 0x61;
                i += 2;
            } else if (next == 0x91) {
                // 'ё'
                buffer[j++] = 0xF2;
                i += 2;
            }             
        } else if (c < 0x80) {
            // ASCII
            buffer[j++] = c;
            i++;
        } else {
            // Неизвестный символ
            buffer[j++] = '?';
            i++;
        }
    }

    buffer[j] = '\0';
    return buffer;
}

void TroykaOLED::_stamp(int16_t x, int16_t y, uint64_t body, uint8_t color) {
    if (x >= 0 && x < _width && y >= 0 && y < _height) {
        uint8_t page = y / 8;
        uint8_t bit = y % 8;

        uint16_t index = x * (_height / 8) + page;

        switch (color) {
            case WHITE:
                _screenBuffer.asBytes[index] |= (1 << bit);
                break;
            case BLACK:
                _screenBuffer.asBytes[index] &= ~(1 << bit);
                break;
            case INVERSE:
                _screenBuffer.asBytes[index] ^= (1 << bit);
                break;
        }
    }
}

void TroykaOLED::_drawPixel(int16_t x, int16_t y, uint8_t color) {
    _stamp(x, y, 1, color);
    _markChangedColumns(x - 1, x + 1);
}

void TroykaOLED::_drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) {
    int16_t dx = x2 - x1;
    int16_t dy = y2 - y1;

    if (abs(dx) > abs(dy)) {
        if (x1 < x2) {
            for (int16_t x = x1; x <= x2; x++) {
                _drawPixel(x, y1 + (dy * (x - x1)) / dx, color);
            }
        } else {
            for (int16_t x = x1; x >= x2; x--) {
                _drawPixel(x, y1 + (dy * (x - x1)) / dx, color);
            }
        }
    } else {
        if (y1 < y2) {
            for (int16_t y = y1; y <= y2; y++) {
                _drawPixel(x1 + (dx * (y - y1)) / dy, y, color);
            }
        } else {
            for (int16_t y = y1; y >= y2; y--) {
                _drawPixel(x1 + (dx * (y - y1)) / dy, y, color);
            }
        }
    }
}

void TroykaOLED::_sendCommand(uint8_t command) {
    char cmd[] = {0x80, command};
    _i2c->write(_i2cAddress << 1, cmd, 2);
}

void TroykaOLED::_sendCommand(uint8_t command, uint8_t value) {
    _sendCommand(command);
    _sendCommand(value);
}

void TroykaOLED::_sendCommand(uint8_t command, uint8_t value1, uint8_t value2) {
    _sendCommand(command);
    _sendCommand(value1);
    _sendCommand(value2);
}

void TroykaOLED::_markChangedColumns(int16_t left, int16_t right) {
    if (left < _changedColumns.left) _changedColumns.left = left;
    if (right > _changedColumns.right) _changedColumns.right = right;
}

void TroykaOLED::_sendBuffer() {
    _sendCommand(SSD1306_ADDR_PAGE, 0, _height / 8 - 1);
    _sendCommand(SSD1306_ADDR_COLUMN, 0, _width - 1);

    for (size_t i = 0; i < sizeof(_screenBuffer.asBytes); ) {
        char buffer[17] = {0x40};
        for (size_t j = 0; j < 16 && i < sizeof(_screenBuffer.asBytes); ++j) {
            buffer[j + 1] = _screenBuffer.asBytes[i++];
        }
        _i2c->write(_i2cAddress << 1, buffer, 17);
    }
}

void TroykaOLED::_sendColumns(uint8_t start, uint8_t end) {
    _sendCommand(SSD1306_ADDR_PAGE, 0, _height / 8 - 1);
    _sendCommand(SSD1306_ADDR_COLUMN, start, end);

    for (size_t i = start * 8; i < static_cast<size_t>((end + 1) * 8); ) {
        char buffer[9] = {0x40};
        for (uint8_t j = 0; j < 8 && i < sizeof(_screenBuffer.asBytes); ++j) {
            buffer[j + 1] = _screenBuffer.asBytes[i++];
        }
        _i2c->write(_i2cAddress << 1, buffer, 9);
    }
}