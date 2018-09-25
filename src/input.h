#ifndef INPUT_H
#define INPUT_H

#include <cstdint>

class Input
{
public:
    enum ControllerState {
        Up =                0x01,
        Down =              0x02,
        Left =              0x04,
        Right =             0x08,
        A =                 0x10,
        B =                 0x20,
        C =                 0x40,
        D =                 0x80,
        Controller1Start =  0x01,
        Controller1Select = 0x02,
        Controller2Start =  0x04,
        Controller2Select = 0x08
    };

    Input();

    void reset();

    void setInput(uint8_t new1, uint8_t new2, uint8_t new3);

    uint8_t input1;
    uint8_t input2;
    uint8_t input3;
    uint8_t selector;
};

#endif // INPUT_H
