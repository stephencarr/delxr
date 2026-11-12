/* Delxr native LXR sound editor. SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include "gui/ui/ui.h"

class Sound;
class Source;

class LxrEditor final : public UI {
public:
	bool tryOpen();
	bool opened() override;
	bool canSeeViewUnderneath() override { return true; }
	UIType getUIType() override { return UIType::NONE; }
	ActionResult buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) override;
	ActionResult padAction(int32_t x, int32_t y, int32_t velocity) override;
	ActionResult horizontalEncoderAction(int32_t offset) override;
	ActionResult verticalEncoderAction(int32_t offset, bool inCardRoutine) override;
	void modEncoderAction(int32_t which, int32_t offset) override;
	void modEncoderButtonAction(uint8_t which, bool on) override;
	void modButtonAction(uint8_t which, bool on) override;
	ActionResult exitUI() override;
	void renderOLED(deluge::hid::display::oled_canvas::Canvas& canvas) override;

private:
	struct Page { const char* title; int top; int bottom; };
	Sound* selectedSound() const;
	Source* selectedSource();
	void rebuildPages();
	void refresh();
	int parameterForKnob(int which) const;
	int primaryParamId(int control) const;
	int parameterValue(int parameter);
	const char* parameterLabel(int parameter);
	void parameterText(int parameter, char* text, int capacity);
	Page pages_[20]{};
	int pageCount_ = 0;
	int page_ = 0;
	int source_ = 0;
};

extern LxrEditor lxrEditor;
