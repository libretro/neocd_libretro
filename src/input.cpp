#include "input.h"

Input::Input() :
    input1(0xFF),
    input2(0xFF),
    input3(0x0F),
    selector(0)
{
    reset();
}

void Input::reset()
{
    input1 = 0xFF;
    input2 = 0xFF;
    input3 = 0x0F;
    selector = 0;
}

void Input::setInput(uint8_t new1, uint8_t new2, uint8_t new3)
{
    input1 = new1;
    input2 = new2;
    input3 = new3 & 0x0F;
}
