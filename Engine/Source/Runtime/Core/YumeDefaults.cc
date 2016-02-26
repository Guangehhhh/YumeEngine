//----------------------------------------------------------------------------
//Yume Engine
//Copyright (C) 2015  arkenthera
//This program is free software; you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation; either version 2 of the License, or
//(at your option) any later version.
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//You should have received a copy of the GNU General Public License along
//with this program; if not, write to the Free Software Foundation, Inc.,
//51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.*/
//----------------------------------------------------------------------------
//
// File : <Filename> YumeGraphics.h
// Date : 2.19.2016
// Comments :
//
//----------------------------------------------------------------------------
#include "YumeHeaders.h"
#include "YumeDefaults.h"
#include "YumeFile.h"

#include "YumeXmlParser.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/format.hpp>

#include <cstdio>
#include <fcntl.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#if defined(_MSC_VER)
#include <float.h>
#else
// From http://stereopsis.com/FPU.html

#define FPU_CW_PREC_MASK        0x0300
#define FPU_CW_PREC_SINGLE      0x0000
#define FPU_CW_PREC_DOUBLE      0x0200
#define FPU_CW_PREC_EXTENDED    0x0300
#define FPU_CW_ROUND_MASK       0x0c00
#define FPU_CW_ROUND_NEAR       0x0000
#define FPU_CW_ROUND_DOWN       0x0400
#define FPU_CW_ROUND_UP         0x0800
#define FPU_CW_ROUND_CHOP       0x0c00

inline unsigned GetFPUState()
{
	unsigned control = 0;
	__asm__ __volatile__ ("fnstcw %0" : "=m" (control));
	return control;
}

inline void SetFPUState(unsigned control)
{
	__asm__ __volatile__ ("fldcw %0" : : "m" (control));
}

#endif

#include <SDL.h>

namespace YumeEngine
{
	static YumeVector<YumeString>::type arguments;

	void CreateConfigFile(const boost::filesystem::path& path)
	{
		pugi::xml_document doc;

		XmlNode root = doc.append_child("Yume");
		XmlNode graphics = root.append_child("Graphics");
		XmlNode renderer = graphics.append_child("Renderer");
		XmlNode fullscreen = graphics.append_child("Fullscreen");
		XmlNode width = graphics.append_child("WindowWidth");
		XmlNode height = graphics.append_child("WindowHeight");
		XmlNode borderlessWindow = graphics.append_child("BorderlessWindow");
		XmlNode vsync = graphics.append_child("Vsync");
		XmlNode tripleBuffer = graphics.append_child("TripleBuffer");
		XmlNode multisample = graphics.append_child("MultiSample");

		//Set default values
		renderer.text().set("Direct3D11");
		fullscreen.text().set(0);
		width.text().set(1280);
		height.text().set(720);
		borderlessWindow.text().set(0);
		vsync.text().set(0);
		tripleBuffer.text().set(0);
		multisample.text().set(1);
		fullscreen.text().set(0);


		doc.save_file(path.generic_wstring().c_str());
	}

	const YumeVector<YumeString>::type& ParseArguments(const YumeString& cmdLine,bool skipFirst)
	{
		arguments.clear();

		unsigned cmdStart = 0,cmdEnd = 0;
		bool inCmd = false;
		bool inQuote = false;

		for(unsigned i = 0; i < cmdLine.size(); ++i)
		{
			if(cmdLine[i] == '\"')
				inQuote = !inQuote;
			if(cmdLine[i] == ' ' && !inQuote)
			{
				if(inCmd)
				{
					inCmd = false;
					cmdEnd = i;
					// Do not store the first argument (executable name)
					if(!skipFirst)
						arguments.push_back(cmdLine.substr(cmdStart,cmdEnd - cmdStart));
					skipFirst = false;
				}
			}
			else
			{
				if(!inCmd)
				{
					inCmd = true;
					cmdStart = i;
				}
			}
		}
		if(inCmd)
		{
			cmdEnd = cmdLine.size();
			if(!skipFirst)
				arguments.push_back(cmdLine.substr(cmdStart,cmdEnd - cmdStart));
		}
		for(unsigned i = 0; i < arguments.size(); ++i)
		{
			boost::replace_all(arguments[i],"\"","");
			boost::replace_all(arguments[i],"--","");
		}

		return arguments;
	}

	const YumeVector<YumeString>::type& ParseArguments(const char* cmdLine)
	{
		return ParseArguments(YumeString(cmdLine));
	}

	const YumeVector<YumeString>::type& ParseArguments(int argc,char** argv)
	{
		YumeString cmdLine;

		for(int i = 0; i < argc; ++i)
		{
			boost::format fmt = boost::format("\"%s\" ") % (const char*)argv[i];
			cmdLine.append(fmt.str());
		}

		return ParseArguments(cmdLine);
	}

	void InitFPU()
	{
#ifdef _MSC_VER
		_controlfp(_RC_NEAR | _PC_24,_MCW_RC | _MCW_PC);
#else
		unsigned control = GetFPUState();
		control &= ~(FPU_CW_PREC_MASK | FPU_CW_ROUND_MASK);
		control |= (FPU_CW_PREC_SINGLE | FPU_CW_ROUND_NEAR);
		SetFPUState(control);
#endif
	}

	void ErrorDialog(const YumeString& title,const YumeString& message)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,title.c_str(),message.c_str(),0);
	}

	void ErrorExit(const YumeString& message,int exitCode)
	{
		if(!message.empty())
			ErrorDialog("Error!",message);

		exit(exitCode);
	}


	const YumeVector<YumeString>::type& GetArguments()
	{
		return arguments;
	}

}
