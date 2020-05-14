#include <algorithm>

#include "cdrom.h"
#include "cdromcontroller.h"
#include "neogeocd.h"

//#define CONTROLLER_LOG(x, ...) LOG(x, __VA_ARGS__)
#define CONTROLLER_LOG(x, ...)

CdromController::CdromController() :
    status(CdIdle)
{ }

void CdromController::reset()
{

}
    
void CdromController::processCdCommand()
{
    const auto& commandPacket = neocd.lc8951.commandPacket;
    auto& responsePacket = neocd.lc8951.responsePacket;

    if (commandPacket[0])
    {
        CONTROLLER_LOG(LOG_INFO, "COMMAND %02X%02X%02X%02X%02X\n",
            commandPacket[0],
            commandPacket[1],
            commandPacket[2],
            commandPacket[3],
            commandPacket[4]);
    }

    if ((commandPacket[4] & 0x0F) != neocd.lc8951.calculatePacketChecksum(commandPacket))
    {
        CONTROLLER_LOG(LOG_INFO, "CD command with wrong checksum!");
        responsePacket[0] = status;
        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;
        neocd.lc8951.setPacketChecksum(responsePacket);
        return;
    }

    switch (commandPacket[0])
    {
    case 0x00:  // Status
        responsePacket[0] = (responsePacket[0] & 0x0F) | status;
        break;

    case 0x10:  // Stop
        neocd.cdrom.stop();
        status = CdIdle;
        responsePacket[0] = status;
        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;
        break;

    case 0x20:  // Query info
        if ((status == CdIdle) && (!neocd.cdrom.isTocEmpty()))
            status = CdStopped;

        switch (commandPacket[1] & 0x0F)    // Sub command
        {
        case 0x00:  // Get current position
        {
            uint32_t     m, s, f;
            CdromToc::toMSF(CdromToc::fromLBA(neocd.cdrom.position()), m, s, f);
            responsePacket[0] = status;
            responsePacket[1] = Cdrom::toBCD(m);
            responsePacket[2] = Cdrom::toBCD(s);
            responsePacket[3] = Cdrom::toBCD(f);
            responsePacket[4] = neocd.cdrom.isData() ? 0x40 : 0x00;
        }
        break;

        case 0x01:  // Get current position (relative)
        {
            uint32_t position;

            if (neocd.cdrom.isPregap()) // Pregaps are counted down
                position = (neocd.cdrom.currentTrackPosition() + neocd.cdrom.currentIndexSize()) - (neocd.cdrom.position() + 1);
            else
                position = neocd.cdrom.position() - neocd.cdrom.currentTrackPosition();

            uint32_t m, s, f;
            CdromToc::toMSF(position, m, s, f);

            responsePacket[0] = status | 0x01;
            responsePacket[1] = Cdrom::toBCD(m);
            responsePacket[2] = Cdrom::toBCD(s);
            responsePacket[3] = Cdrom::toBCD(f);
            responsePacket[4] = neocd.cdrom.isData() ? 0x40 : 0x00;
        }
        break;

        case 0x02:  // Get current track
        {
            /*
                NOTE: Copy protection can be simulated by generating indexes for track 01 as follow:
                index = std::min(99, (((m * 100) + s) / 4) + 1);
                */
            TrackIndex trackIndex = neocd.cdrom.currentTrackIndex();
            responsePacket[0] = status | 0x02;
            responsePacket[1] = Cdrom::toBCD(trackIndex.track());
            responsePacket[2] = Cdrom::toBCD(trackIndex.index());
            responsePacket[3] = 0x00;
            responsePacket[4] = neocd.cdrom.isData() ? 0x40 : 0x00;
        }
        break;

        case 0x03: // Get leadout address (msf)
        {
            uint32_t m, s, f;
            CdromToc::toMSF(CdromToc::fromLBA(neocd.cdrom.leadout()), m, s, f);
            responsePacket[0] = status | 0x03;
            responsePacket[1] = Cdrom::toBCD(m);
            responsePacket[2] = Cdrom::toBCD(s);
            responsePacket[3] = Cdrom::toBCD(f);
            responsePacket[4] = 0x00;
        }
        break;

        case 0x04: // Get first track and last track
        {
            responsePacket[0] = status | 0x04;
            responsePacket[1] = Cdrom::toBCD(neocd.cdrom.firstTrack());
            responsePacket[2] = Cdrom::toBCD(neocd.cdrom.lastTrack());
            responsePacket[3] = 0x00;
            responsePacket[4] = 0x00;
        }
        break;

        case 0x05: // Get track info
        {
            uint8_t track = Cdrom::fromBCD(commandPacket[2]);
            uint32_t position = CdromToc::fromLBA(neocd.cdrom.trackPosition(track));
            uint32_t m, s, f;
            CdromToc::toMSF(position, m, s, f);

            responsePacket[0] = status | 0x05;
            responsePacket[1] = Cdrom::toBCD(m);
            responsePacket[2] = Cdrom::toBCD(s);

            if (neocd.cdrom.trackIsData(track))
                responsePacket[3] = Cdrom::toBCD(f) | 0x80;
            else
                responsePacket[3] = Cdrom::toBCD(f);

            responsePacket[4] = commandPacket[2] << 4;
        }
        break;

        case 0x06: // Check end of disc?
        {
            if (neocd.cdrom.position() >= neocd.cdrom.leadout())
                status = CdEndOfDisc;

            responsePacket[0] = status | 0x06;
            responsePacket[1] = 0x00;
            responsePacket[2] = 0x00;
            responsePacket[3] = 0x00;
            responsePacket[4] = neocd.cdrom.isData() ? 0x40 : 0x00;
        }
        break;

        case 0x07: // Unknown, CDZ copy protection related. (Disc recognition)
        {
            responsePacket[0] = status | 0x07;
            responsePacket[1] = 0x02; // Possible values: 2, 5, E, F. Most games want 2, Twinkle Star Sprites want F
            responsePacket[2] = 0x00;
            responsePacket[3] = 0x00;
            responsePacket[4] = 0x00;
        }
        break;

        default:
            CONTROLLER_LOG(LOG_INFO, "CDROM: Received unknown 0x20 sub-command (%02X%02X%02X%02X%02X)\n",
                commandPacket[0],
                commandPacket[1],
                commandPacket[2],
                commandPacket[3],
                commandPacket[4]);
            responsePacket[0] = status;
            responsePacket[1] = 0x00;
            responsePacket[2] = 0x00;
            responsePacket[3] = 0x00;
            responsePacket[4] = 0x00;
            break;
        }
        break;

    case 0x30: // Play
    {
        uint8_t m, s, f;

        m = Cdrom::fromBCD(commandPacket[1]);
        s = Cdrom::fromBCD(commandPacket[2]);
        f = Cdrom::fromBCD(commandPacket[3]);

        uint32_t position = CdromToc::toLBA(CdromToc::fromMSF(m, s, f));

        /*
            CDZ Bios sometimes send the play command twice.
            There is a command FIFO at $7472(A5)
            Read pointer is $7650(A5) and write pointer $7651(A5)
            The code at $C0B4B2 writes 7 (Send play command) to the FIFO and is systematically called twice.
            Some tracks use this, other tracks use $C0B3C6 (called once)
            Is this a bug in the BIOS? Or is something not properly emulated?
        */
        neocd.cdrom.play();
        neocd.cdrom.seek(position);
//        updateHeadRegisters(neocd.cdrom.position());

        status = CdPlaying;
        responsePacket[0] = status | 0x02;
        responsePacket[1] = Cdrom::toBCD(neocd.cdrom.currentTrackIndex().track());
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;
    }
        break;

    case 0x40: // Seek
        CONTROLLER_LOG(LOG_INFO, "CDROM: Received seek command, ignored (%02X%02X%02X%02X%02X)\n",
            commandPacket[0],
            commandPacket[1],
            commandPacket[2],
            commandPacket[3],
            commandPacket[4]);

        neocd.cdrom.stop();
        status = CdPaused;
        responsePacket[0] = CdSeeking;
        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;
        break;

    case 0x50: // Unknown, CDZ only
        responsePacket[0] = status;
        break;

    case 0x60: // Pause
        neocd.cdrom.stop();

        status = CdPaused;
        responsePacket[0] = status;
        break;

    case 0x70: // Resume
        neocd.cdrom.play();

        status = CdPlaying;
        responsePacket[0] = status;
        break;

    case 0x80: // Scan forward
    {
        uint32_t position = neocd.cdrom.position();

        position = std::min(position + SCAN_SPEED, neocd.cdrom.leadout() - 1);

        neocd.cdrom.seek(position);

        status = CdPlaying;
        responsePacket[0] = CdScanning;
    }
        break;

    case 0x90: // Scan backward
    {
        uint32_t position = neocd.cdrom.position();

        if (position < SCAN_SPEED)
            position = 0;
        else
            position -= SCAN_SPEED;

        neocd.cdrom.seek(position);

        status = CdPlaying;
        responsePacket[0] = CdScanning;
    }
        break;

    case 0xB0: // Move to track
    {
        uint8_t track = Cdrom::fromBCD(commandPacket[1]);
        uint32_t position = neocd.cdrom.trackPosition(track);

        neocd.cdrom.play();
        neocd.cdrom.seek(position);

        status = CdPlaying;
        responsePacket[0] = status | 0x02;
        responsePacket[1] = Cdrom::toBCD(neocd.cdrom.currentTrackIndex().track());
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;
    }
        break;

    case 0x02: // More CD Protection Stuff?
    case 0x13: // The results of those are written to $778C(A5)
    case 0x23: // Then transferred to $800076 which is in the backup RAM... What for?
    case 0x33:
    case 0x43:
    case 0x53:
    case 0x63:
    case 0xE2:
        responsePacket[0] = status;
        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;
        break;

    default:
      CONTROLLER_LOG(LOG_INFO, "COMMAND Received unknown command %02X%02X%02X%02X%02X\n",
            commandPacket[0],
            commandPacket[1],
            commandPacket[2],
            commandPacket[3],
            commandPacket[4]);
        responsePacket[0] = status;
        responsePacket[1] = 0x00;
        responsePacket[2] = 0x00;
        responsePacket[3] = 0x00;
        responsePacket[4] = 0x00;
        break;
    }

    neocd.lc8951.setPacketChecksum(responsePacket);

/*  CONTROLLER_LOG(LOG_INFO, "ANSWER  %02X%02X%02X%02X%02X\n",
        responsePacket[0],
        responsePacket[1],
        responsePacket[2],
        responsePacket[3],
        responsePacket[4]);*/
}
