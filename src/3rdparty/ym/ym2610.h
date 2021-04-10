/***************************************************************************
 *   NeoCD - The Neo Geo CD emulator                                       *
 *   Copyright (C) 2008 by Fabrice Martinez                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef _YM2610_H_
#define _YM2610_H_

#include <cstdint>

class DataPacker;

/*
 for busy flag emulation , function FM_GET_TIME_NOW() should be
 return the present time in second unit with (double) value
 in timer.c
 */
#define FM_GET_TIME_NOW() neocd.z80CurrentTimeSeconds()

typedef int16_t FMSAMPLE;
typedef int32_t FMSAMPLE_MIX;

typedef void(*FM_TIMERHANDLER) (int channel, int count, double stepTime);
typedef void(*FM_IRQHANDLER) (int irq);

int YM2610Init(int baseclock, int rate,
	void *pcmroma, int pcmsizea,
#if YM2610_ADPCMB
	void *pcmromb, int pcmsizeb,
#endif
	FM_TIMERHANDLER TimerHandler,
	FM_IRQHANDLER IRQHandler);

void  YM2610Reset(void);
void  YM2610Update(int length);
int   YM2610Write(int addr, uint8_t value);
uint8_t YM2610Read(int addr);
int   YM2610TimerOver(int channel);

bool YM2610SaveState(DataPacker& out);
bool YM2610RestoreState(DataPacker& in);

#endif /* _YM2610_H_ */
