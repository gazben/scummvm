/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

/*
 * This code is based on original Tony Tough source code
 *
 * Copyright (c) 1997-2003 Nayma Software
 */

#ifndef TONY_WINDOW_H
#define TONY_WINDOW_H

#include "common/scummsys.h"
#include "common/rect.h"
#include "tony/adv.h"

namespace Tony {

typedef uint32 HWND;
struct DDSURFACEDESC {
};

class RMSnapshot {
private:
	// Buffer used to create a path
	static char bufDrive[MAX_DRIVE], bufDir[MAX_DIR], bufName[MAX_FNAME], bufExt[MAX_EXT];
	static char filename[512];

	// Buffer used to convert a RGB
	static byte	rgb[RM_SX * RM_SY * 3];

private:
	bool GetFreeSnapName(char *fn);

public:
	// Take a screenshot
	void GrabScreenshot(byte *lpBuf, int dezoom = 1, uint16 *lpDestBuf = NULL);
};


class RMWindow {
private:
	bool Lock();
	void Unlock();
	void plotSplices(const byte *lpBuf, const Common::Point &center, int x, int y);
	void plotLines(const byte *lpBuf, const Common::Point &center, int x, int y);

protected:
	void * /*LPDIRECTDRAWCLIPPER*/ m_MainClipper;
	void * /*LPDIRECTDRAWCLIPPER*/ m_BackClipper;

	int fps, fcount;
	int lastsecond, lastfcount;

	int mskRed, mskGreen, mskBlue;

	bool m_bGrabScreenshot;
	bool m_bGrabThumbnail;
	bool m_bGrabMovie;
	uint16 *m_wThumbBuf;

	void CreateBWPrecalcTable(void);
	void WipeEffect(Common::Rect &rcBoundEllipse);
	void GetNewFrameWipe(byte *lpBuf, Common::Rect &rcBoundEllipse);

public:
	RMWindow();
	~RMWindow();

	// Initialization
	void Init(/*HINSTANCE hInst*/);
	void InitDirectDraw(void);
	void Close(void);

	// Repaint grafico tramite DirectDraw
	void Repaint(void);

	// Switch tra windowed e fullscreen
	void SwitchFullscreen(bool bFull) {}

	// Legge il prossimo frame
	void GetNewFrame(byte *lpBuf, Common::Rect *rcBoundEllipse);

	// Avverte di grabbare un thumbnail per il salvataggio
	void GrabThumbnail(uint16 *buf);

	int getFps() const { return fps; }
};

} // End of namespace Tony

#endif /* TONY_WINDOW_H */