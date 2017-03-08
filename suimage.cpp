/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This file is part of Suneido - The Integrated Application Platform
 * see: http://www.suneido.com for more information.
 *
 * Copyright (c) 2000 Suneido Software Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation - version 2.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License in the file COPYING
 * for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// uses OleLoadPicture as in the MSDN loadpic sample (Q218972)

// TODO: maybe use OleLoadPicturePath for files

#include "builtinclass.h"
#include "gcstring.h"
#include "win.h"
#include <olectl.h>
#include "sufinalize.h"

class SuImage : public SuFinalize
	{
public:
	void init(const gcstring& data);
	virtual void out(Ostream& os)
		{
		if (filename == "")
			os << "anImage";
		else
			os << "Image('" << filename << "')";
		}
	static Method<SuImage>* methods()
		{
		static Method<SuImage> methods[] =
			{
			Method<SuImage>("Close", &SuImage::Close),
			Method<SuImage>("Height", &SuImage::Height),
			Method<SuImage>("Width", &SuImage::Width),
			Method<SuImage>("Draw", &SuImage::Draw),
			Method<SuImage>("", 0)
			};
		return methods;
		}
	const char* type() const
		{ return "Image"; }
private:
	Value Width(BuiltinArgs&);
	Value Height(BuiltinArgs&);
	Value Draw(BuiltinArgs&);
	Value Close(BuiltinArgs&);

	virtual void finalize();
	static bool isfilename(const gcstring& s);
	static HGLOBAL readfile(const char* filename, DWORD* pdwSize);
	void ckopen();

	LPPICTURE gpPicture;
	gcstring filename;
	};

Value su_image()
	{
	static BuiltinClass<SuImage> suImageClass("(image)");
	return &suImageClass;
	}

template<>
void BuiltinClass<SuImage>::out(Ostream& os)
	{
	os << "Image /* builtin class */";
	}

const int HIMETRIC_INCH = 2540;

template<>
Value BuiltinClass<SuImage>::instantiate(BuiltinArgs& args)
	{
	args.usage("usage: Image(image)");
	gcstring data = args.getgcstr("image");
	args.end();
	SuImage* image = new BuiltinInstance<SuImage>();
	image->init(data);
	return image;
	}

void SuImage::init(const gcstring& data)
	{
	DWORD dwSize;
	HGLOBAL hGlobal;
	if (isfilename(data))
		{
		filename = data;
		hGlobal = readfile(data.str(), &dwSize);
		}
	else
		{
		dwSize = data.size();
		hGlobal = GlobalAlloc(GMEM_MOVEABLE, dwSize);
		verify(NULL != hGlobal);
		LPVOID pvData = GlobalLock(hGlobal);
		verify(NULL != pvData);
		memcpy(pvData, data.buf(), dwSize);
		GlobalUnlock(hGlobal);
		}

	LPSTREAM pstm = NULL;
	// create IStream* from global memory
	HRESULT hr = CreateStreamOnHGlobal(hGlobal, TRUE, &pstm);
	if (! SUCCEEDED(hr) && pstm)
		{
		GlobalFree(hGlobal);           // Otherwise, fDeleteOnRelease = TRUE will do this
		verify(SUCCEEDED(hr) && pstm); // Throw assertion failure to Suneido programmer
		}

	// Create IPicture from image file
	hr = ::OleLoadPicture(pstm, dwSize, FALSE, IID_IPicture, (LPVOID *)&gpPicture);
	if (! SUCCEEDED(hr) || ! gpPicture)
		except("Image: couldn't load picture");
	pstm->Release();
	}

bool SuImage::isfilename(const gcstring& arg)
	{
	if (arg.size() > 256)
		return false;
	for (const char *s = arg.begin(), *end = arg.end(); s < end; ++s)
		if (*s < ' ' || '~' < *s)
			return false;
	return true;
	}

HGLOBAL SuImage::readfile(const char* filename, DWORD* pdwSize)
	{
	// open file
	HANDLE hFile = CreateFile(filename, GENERIC_READ, 0, NULL,
		OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		except("Image: can't open " << filename);

	// get file size
	DWORD dwFileSize = GetFileSize(hFile, NULL);
	verify(-1 != dwFileSize);

	LPVOID pvData = NULL;
	// alloc memory based on file size
	HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, dwFileSize);
	verify(NULL != hGlobal);

	pvData = GlobalLock(hGlobal);
	verify(NULL != pvData);

	DWORD dwBytesRead = 0;
	// read file and store in global memory
	BOOL bRead = ReadFile(hFile, pvData, dwFileSize, &dwBytesRead, NULL);
	verify(FALSE != bRead);
	GlobalUnlock(hGlobal);
	CloseHandle(hFile);

	*pdwSize = dwFileSize;
	return hGlobal;
	}

Value SuImage::Draw(BuiltinArgs& args)
	{
	args.usage("usage: image.Draw(hdc, x, y, w, h)");
	HDC hdc = (HDC) args.getint("hdc");
	int x = args.getint("x");
	int y = args.getint("y");
	int w = args.getint("w", 0);
	int h = args.getint("h", 0);
	args.end();

	ckopen();

	// get width and height of picture
	long hmWidth;
	long hmHeight;
	gpPicture->get_Width(&hmWidth);
	gpPicture->get_Height(&hmHeight);
	// convert himetric to pixels
	int nWidth	= MulDiv(hmWidth, GetDeviceCaps(hdc, LOGPIXELSX), HIMETRIC_INCH);
	int nHeight	= MulDiv(hmHeight, GetDeviceCaps(hdc, LOGPIXELSY), HIMETRIC_INCH);
	if (w == 0)
		w = nWidth;
	if (h == 0)
		h = nHeight;
	RECT rc;
	rc.left = x;
	rc.top = y;
	rc.right = x + w;
	rc.bottom = y + h;
	// display picture using IPicture::Render
	gpPicture->Render(hdc, x, y, w, h, 0, hmHeight - 1, hmWidth, -hmHeight, &rc);
	return Value();
	}

Value SuImage::Width(BuiltinArgs& args)
	{
	args.usage("usage: image.Width(hdc = false)");
	HDC hdc = (HDC) args.getint("hdc", 0);
	args.end();

	ckopen();
	long hmWidth;
	gpPicture->get_Width(&hmWidth);
	if (hdc == 0)
		return hmWidth;
	// convert himetric to pixels
	return MulDiv(hmWidth, GetDeviceCaps(hdc, LOGPIXELSX), HIMETRIC_INCH);
	}

Value SuImage::Height(BuiltinArgs& args)
	{
	args.usage("usage: image.Width(hdc = false)");
	HDC hdc = (HDC) args.getint("hdc", 0);
	args.end();

	ckopen();
	long hmHeight;
	gpPicture->get_Height(&hmHeight);
	if (hdc == 0)
		return hmHeight;
	// convert himetric to pixels
	return MulDiv(hmHeight, GetDeviceCaps(hdc, LOGPIXELSY), HIMETRIC_INCH);
	}

Value SuImage::Close(BuiltinArgs& args)
	{
	args.usage("usage: image.Close()");
	args.end();

	ckopen();
	finalize();
	removefinal();
	return Value();
	}

void SuImage::ckopen()
	{
	if (! gpPicture)
		except("Image: already closed");
	}

void SuImage::finalize()
	{
	if (gpPicture)
		gpPicture->Release();
	gpPicture = 0;
	}
