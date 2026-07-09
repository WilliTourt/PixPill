#pragma once

#include <cstdint>

class SimBase {
    public:
        enum class Status {
            OK,
            ERR_ACCEL,
            ERR_LED,
            ERR
        };

        virtual ~SimBase() = default;
        virtual Status init() = 0;
        virtual Status calc() = 0;
        virtual Status draw() = 0;
};
