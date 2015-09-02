///////////////////////////////////////////////////////////////////////////////////
/// Yume Engine MIT License (MIT)

/// Copyright (c) 2015 Alperen Gezer

/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
/// 
/// The above copyright notice and this permission notice shall be included in
/// all copies or substantial portions of the Software.
/// 
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
/// THE SOFTWARE.
/// 
/// File : YumeRenderer.h
/// Date : 8.31.2015
/// Comments : 
///
///////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------
#ifndef __YumeRenderer_h__
#define __YumeRenderer_h__
//---------------------------------------------------------------------------------
#include "YumeRequired.h"
#include "YumeCommon.h"

#include "YumeRendererCapabilities.h"
#include "YumeRenderTarget.h"
//---------------------------------------------------------------------------------
namespace YumeEngine
{
	class YumeAPIExport YumeRenderer : public RenderObjAlloc
	{
	public:
		YumeRenderer();

		virtual ~YumeRenderer();

		virtual const YumeString& GetName();

		virtual YumeRenderWindow* CreateRenderWindow(bool autoCreate, const YumeString& Title = "Yume Engine");
	};
}
//---------------------------------------------------------------------------------
#endif
//~End of YumeRenderer.h