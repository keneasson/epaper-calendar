/*
 * Arduino API Simulation for Native Builds
 * Provides Arduino-compatible types and functions for desktop testing.
 */

#ifndef ARDUINO_SIM_H
#define ARDUINO_SIM_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <string>

// Arduino types
using byte = uint8_t;
using word = uint16_t;

// Pin modes
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

// Digital values
#define HIGH 1
#define LOW 0

// Time functions
inline unsigned long millis() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

inline unsigned long micros() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
}

inline void delay(unsigned long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline void delayMicroseconds(unsigned int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

// Pin functions (no-op in simulation)
inline void pinMode(uint8_t pin, uint8_t mode) {
    (void)pin; (void)mode;
}

inline void digitalWrite(uint8_t pin, uint8_t val) {
    (void)pin; (void)val;
}

inline int digitalRead(uint8_t pin) {
    (void)pin;
    return LOW;
}

// Simulated battery ADC - returns value representing ~3.8V
inline int analogRead(uint8_t pin) {
    (void)pin;
    // Simulate ~3.8V battery (mid-charge)
    // With voltage divider of 2.0: 3.8V / 2 = 1.9V
    // ADC: 1.9V / 3.3V * 4095 = ~2358
    return 2358 + (rand() % 50 - 25);  // Add some noise
}

// String class (simplified)
class String {
public:
    String() : data_("") {}
    String(const char* s) : data_(s ? s : "") {}
    String(const String& s) : data_(s.data_) {}

    const char* c_str() const { return data_.c_str(); }
    size_t length() const { return data_.length(); }
    bool isEmpty() const { return data_.empty(); }

    String& operator=(const char* s) { data_ = s ? s : ""; return *this; }
    String& operator=(const String& s) { data_ = s.data_; return *this; }
    String operator+(const char* s) const { return String((data_ + s).c_str()); }
    String operator+(const String& s) const { return String((data_ + s.data_).c_str()); }

private:
    std::string data_;
};

// Serial simulation
class SerialClass {
public:
    void begin(unsigned long baud) { (void)baud; }
    void end() {}
    void flush() { fflush(stdout); }

    void print(const char* s) { printf("%s", s); }
    void print(const String& s) { printf("%s", s.c_str()); }
    void print(int n) { printf("%d", n); }
    void print(unsigned int n) { printf("%u", n); }
    void print(long n) { printf("%ld", n); }
    void print(unsigned long n) { printf("%lu", n); }
    void print(float n) { printf("%.2f", n); }
    void print(double n) { printf("%.2f", n); }

    void println() { printf("\n"); }
    void println(const char* s) { printf("%s\n", s); }
    void println(const String& s) { printf("%s\n", s.c_str()); }
    void println(int n) { printf("%d\n", n); }
    void println(unsigned int n) { printf("%u\n", n); }
    void println(long n) { printf("%ld\n", n); }
    void println(unsigned long n) { printf("%lu\n", n); }
    void println(float n) { printf("%.2f\n", n); }
    void println(double n) { printf("%.2f\n", n); }

    template<typename... Args>
    void printf(const char* fmt, Args... args) {
        ::printf(fmt, args...);
    }
};

extern SerialClass Serial;

// IPAddress class
class IPAddress {
public:
    IPAddress() : addr_{0, 0, 0, 0} {}
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : addr_{a, b, c, d} {}

    String toString() const {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d.%d.%d.%d", addr_[0], addr_[1], addr_[2], addr_[3]);
        return String(buf);
    }

private:
    uint8_t addr_[4];
};

#endif // ARDUINO_SIM_H
