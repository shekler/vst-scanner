//-----------------------------------------------------------------------------
// Project     : VST SDK
//
// Category    : Examples
// Filename    : public.sdk/samples/vst/hostchecker/source/hostchecker.h
// Created by  : Steinberg, 04/2012
// Description :
//
//-----------------------------------------------------------------------------
// LICENSE
// (c) 2024, Steinberg Media Technologies GmbH, All Rights Reserved
//-----------------------------------------------------------------------------
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Steinberg Media Technologies nor the names of its
//     contributors may be used to endorse or promote products derived from this
//     software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#pragma once

#include "vstgui/lib/vstguifwd.h"
#include "vstgui/uidescription/icontroller.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "base/source/fobject.h"
#include <functional>
#include <vector>

//------------------------------------------------------------------------
namespace Steinberg {
namespace Vst {

using SizeFactors = std::vector<float>;
static const SizeFactors kSizeFactors = {0.75f, 1.f, 1.5f};

class EditorSizeController : public FObject, public VSTGUI::IController
{
public:
//------------------------------------------------------------------------
	using SizeFunc = std::function<void (float)>;
	EditorSizeController (EditController* editController, const SizeFunc& sizeFunc, double currentSizeFactor);
	~EditorSizeController () override;

	static const int32_t kSizeParamTag = 2000;

	void PLUGIN_API update (FUnknown* changedUnknown, int32 message) override;
	VSTGUI::CView* verifyView (VSTGUI::CView* view, const VSTGUI::UIAttributes& attributes,
	                           const VSTGUI::IUIDescription* description) override;
	void valueChanged (VSTGUI::CControl* pControl) override;
	void controlBeginEdit (VSTGUI::CControl* pControl) override;
	void controlEndEdit (VSTGUI::CControl* pControl) override;

	void setSizeFactor (double factor);

	OBJ_METHODS (EditorSizeController, FObject)
//------------------------------------------------------------------------
private:
	VSTGUI::CControl* sizeControl = nullptr;
	RangeParameter* sizeParameter = nullptr;
	SizeFunc sizeFunc;
};

//------------------------------------------------------------------------
} // Vst
} // Steinberg
