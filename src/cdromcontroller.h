#ifndef CDROMCONTROLLER_H
#define CDROMCONTROLLER_H

#include <cstdint>

class CdromController
{
public:
    enum Status
    {
        CdIdle      = 0x00,
        CdPlaying   = 0x10,
        CdSeeking   = 0x20,
        CdScanning  = 0x30,
        CdPaused    = 0x40,
        CdStopped   = 0x90,
        CdEndOfDisc = 0xC0
    };

    static const uint32_t SCAN_SPEED = 30;

    CdromController();

    void reset();
    
    void processCdCommand();

    // Variables to save in savestate
    uint8_t     status;
    // End variables to save in savestate
};

#endif
