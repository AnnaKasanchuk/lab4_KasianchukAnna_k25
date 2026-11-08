#ifndef DATA_H
#define DATA_H

#include <string>
#include <mutex>
#include <stdexcept>

struct alignas(64) Slot {
    mutable std::mutex mtx;
    int value = 0;
};

class SharedData {
private:
    Slot slots[2];

public:
    void set(int field, int value) {
        if (field < 0 || field > 1) {
            throw std::out_of_range("Invalid field index");
        }
        std::lock_guard<std::mutex> lock(slots[field].mtx);
        slots[field].value = value;
    }

    int get(int field) const {
        if (field < 0 || field > 1) {
            throw std::out_of_range("Invalid field index");
        }
        std::lock_guard<std::mutex> lock(slots[field].mtx);
        return slots[field].value;
    }

    explicit operator std::string() const {
        int v0, v1;
        {
            std::lock_guard<std::mutex> lk1(slots[1].mtx);
            std::lock_guard<std::mutex> lk0(slots[0].mtx);
            v0 = slots[0].value;
            v1 = slots[1].value;
        }

        return std::to_string(v0) + " " + std::to_string(v1);
    }
};

#endif