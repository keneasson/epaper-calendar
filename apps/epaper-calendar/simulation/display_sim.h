/*
 * Display Simulation for Native Builds
 * Renders display output to a BMP file with actual text.
 */

#ifndef DISPLAY_SIM_H
#define DISPLAY_SIM_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include "bitmap_font.h"

// Display dimensions - Portrait mode (Waveshare 7.5" V2 rotated)
constexpr int SIM_DISPLAY_WIDTH = 480;
constexpr int SIM_DISPLAY_HEIGHT = 800;

// Colors
constexpr uint16_t GxEPD_BLACK = 0x0000;
constexpr uint16_t GxEPD_WHITE = 0xFFFF;

// Font structure with scale factor
struct GFXfont {
    const char* name;
    int baseHeight;
    int scale;  // Scale factor for 8x8 base font
};

// Simulated fonts with different scales
extern const GFXfont FreeSans9pt7b;
extern const GFXfont FreeSans12pt7b;
extern const GFXfont FreeSansBold12pt7b;
extern const GFXfont FreeSansBold18pt7b;
extern const GFXfont FreeSansBold24pt7b;

class DisplaySimulator {
public:
    DisplaySimulator() : rotation_(0), textColor_(GxEPD_BLACK), cursorX_(0), cursorY_(0), currentFont_(nullptr) {
        buffer_.resize(SIM_DISPLAY_WIDTH * SIM_DISPLAY_HEIGHT, 0xFF);  // White
    }

    void init(unsigned long baud) {
        (void)baud;
        printf("[SIM] Display initialized (%dx%d portrait)\n", SIM_DISPLAY_WIDTH, SIM_DISPLAY_HEIGHT);
    }

    void setRotation(int r) {
        rotation_ = r;
        printf("[SIM] Display rotation: %d\n", r);
    }

    void setTextColor(uint16_t color) {
        textColor_ = color;
    }

    void setFont(const GFXfont* font) {
        currentFont_ = font;
    }

    void setCursor(int x, int y) {
        cursorX_ = x;
        cursorY_ = y;
    }

    int width() const { return SIM_DISPLAY_WIDTH; }
    int height() const { return SIM_DISPLAY_HEIGHT; }

    void setFullWindow() {}
    bool firstPage() { return true; }
    bool nextPage() { return false; }

    void fillScreen(uint16_t color) {
        uint8_t val = (color == GxEPD_WHITE) ? 0xFF : 0x00;
        std::fill(buffer_.begin(), buffer_.end(), val);
    }

    void drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
        // Bresenham's line algorithm
        int dx = abs(x1 - x0);
        int dy = abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1;
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;

        while (true) {
            setPixel(x0, y0, color);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx) { err += dx; y0 += sy; }
        }
    }

    void print(const char* text) {
        if (!text) return;

        int scale = currentFont_ ? currentFont_->scale : 1;
        int charWidth = 8 * scale;
        int charHeight = 8 * scale;

        // Draw each character
        for (const char* p = text; *p; p++) {
            drawChar(cursorX_, cursorY_, *p, scale);
            cursorX_ += charWidth;
        }

        // Log for debugging
        printf("[SIM] Text at (%d,%d) scale %d: %s\n",
               cursorX_ - (int)strlen(text) * charWidth, cursorY_, scale, text);
        textLog_.push_back({cursorX_ - (int)strlen(text) * charWidth, cursorY_, text,
                           currentFont_ ? currentFont_->name : "default"});
    }

    void print(int n) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", n);
        print(buf);
    }

    void print(float n) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f", n);
        print(buf);
    }

    void getTextBounds(const char* text, int x, int y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
        (void)x; (void)y;
        *x1 = 0;
        *y1 = 0;
        int scale = currentFont_ ? currentFont_->scale : 1;
        *w = strlen(text) * 8 * scale;
        *h = 8 * scale;
    }

    void hibernate() {
        printf("[SIM] Display entering hibernate\n");
        saveToBMP("display_output.bmp");
    }

    void saveToBMP(const char* filename) {
        printf("[SIM] Saving display to %s\n", filename);

        FILE* f = fopen(filename, "wb");
        if (!f) {
            printf("[SIM] Error: Could not create %s\n", filename);
            return;
        }

        // BMP header for 24-bit color
        int rowSize = ((SIM_DISPLAY_WIDTH * 3 + 3) / 4) * 4;  // Rows padded to 4-byte boundary
        int imageSize = rowSize * SIM_DISPLAY_HEIGHT;
        int fileSize = 54 + imageSize;

        uint8_t header[54] = {0};
        // BM signature
        header[0] = 'B'; header[1] = 'M';
        // File size
        header[2] = fileSize; header[3] = fileSize >> 8; header[4] = fileSize >> 16; header[5] = fileSize >> 24;
        // Data offset
        header[10] = 54;
        // DIB header size
        header[14] = 40;
        // Width
        header[18] = SIM_DISPLAY_WIDTH; header[19] = SIM_DISPLAY_WIDTH >> 8;
        // Height (negative for top-down)
        int negHeight = -SIM_DISPLAY_HEIGHT;
        header[22] = negHeight; header[23] = negHeight >> 8; header[24] = negHeight >> 16; header[25] = negHeight >> 24;
        // Planes
        header[26] = 1;
        // Bits per pixel
        header[28] = 24;
        // Image size
        header[34] = imageSize; header[35] = imageSize >> 8; header[36] = imageSize >> 16; header[37] = imageSize >> 24;

        fwrite(header, 1, 54, f);

        // Pixel data (top-down due to negative height)
        std::vector<uint8_t> row(rowSize, 0);
        for (int y = 0; y < SIM_DISPLAY_HEIGHT; y++) {
            for (int x = 0; x < SIM_DISPLAY_WIDTH; x++) {
                uint8_t val = buffer_[y * SIM_DISPLAY_WIDTH + x];
                row[x * 3 + 0] = val;  // B
                row[x * 3 + 1] = val;  // G
                row[x * 3 + 2] = val;  // R
            }
            fwrite(row.data(), 1, rowSize, f);
        }

        fclose(f);
        printf("[SIM] Display saved to %s (%dx%d)\n", filename, SIM_DISPLAY_WIDTH, SIM_DISPLAY_HEIGHT);

        // Also save text log
        saveTextLog("display_text.log");
    }

    void saveTextLog(const char* filename) {
        FILE* f = fopen(filename, "w");
        if (!f) return;

        fprintf(f, "Display Text Log (Portrait %dx%d)\n", SIM_DISPLAY_WIDTH, SIM_DISPLAY_HEIGHT);
        fprintf(f, "=================================\n\n");

        for (const auto& entry : textLog_) {
            fprintf(f, "[%d, %d] (%s): %s\n",
                    entry.x, entry.y, entry.font.c_str(), entry.text.c_str());
        }

        fclose(f);
    }

private:
    void setPixel(int x, int y, uint16_t color) {
        if (x >= 0 && x < SIM_DISPLAY_WIDTH && y >= 0 && y < SIM_DISPLAY_HEIGHT) {
            buffer_[y * SIM_DISPLAY_WIDTH + x] = (color == GxEPD_WHITE) ? 0xFF : 0x00;
        }
    }

    void drawChar(int x, int y, char c, int scale) {
        const uint8_t* charData = font_get_char(c);

        // Y position is baseline, adjust to top of character
        int startY = y - (8 * scale);

        for (int row = 0; row < 8; row++) {
            uint8_t rowData = charData[row];
            for (int col = 0; col < 8; col++) {
                if (rowData & (0x80 >> col)) {
                    // Draw scaled pixel
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            setPixel(x + col * scale + sx, startY + row * scale + sy, textColor_);
                        }
                    }
                }
            }
        }
    }

    std::vector<uint8_t> buffer_;
    int rotation_;
    uint16_t textColor_;
    int cursorX_, cursorY_;
    const GFXfont* currentFont_;

    struct TextEntry {
        int x, y;
        std::string text;
        std::string font;
    };
    std::vector<TextEntry> textLog_;
};

// GxEPD2 compatibility layer
template<typename T, int H>
class GxEPD2_BW : public DisplaySimulator {
public:
    GxEPD2_BW(T driver) : DisplaySimulator() { (void)driver; }
};

// Display driver type (placeholder)
class GxEPD2_750_T7 {
public:
    static constexpr int HEIGHT = 800;  // Portrait height
    GxEPD2_750_T7(int cs, int dc, int rst, int busy) {
        (void)cs; (void)dc; (void)rst; (void)busy;
    }
};

#endif // DISPLAY_SIM_H
