/* Delxr native LXR sound editor. SPDX-License-Identifier: GPL-3.0-only */
#include "gui/ui/lxr_editor.h"
#include "dsp/delxr/oscillator_type.h"
#include "dsp/lxr/delxr_lxr_patch.h"
#include "gui/views/instrument_clip_view.h"
#include "gui/views/view.h"
#include "hid/buttons.h"
#include "hid/display/display.h"
#include "hid/led/indicator_leds.h"
#include "lib/printf.h"
#include "model/clip/instrument_clip.h"
#include "model/instrument/kit.h"
#include "model/model_stack.h"
#include "model/song/song.h"
#include "modulation/automation/auto_param.h"
#include "processing/sound/sound_drum.h"
#include "processing/sound/sound_instrument.h"
#include <algorithm>

namespace params = deluge::modulation::params;
LxrEditor lxrEditor;

namespace {
constexpr int none = -100;
// Temporarily address one native parameter through the existing gold-encoder path.
// This preserves note-region editing, automation recording, learn-copy/paste and
// Shift-delete without changing the sound's saved knob assignments.
class ScopedKnob {
public:
	ScopedKnob(Sound& sound, int which, int param)
	    : descriptor_(sound.modKnobs[*sound.getModKnobMode()][which].paramDescriptor), saved_(descriptor_) {
		descriptor_.setToHaveParamOnly(param);
	}
	~ScopedKnob() { descriptor_ = saved_; }
private:
	ParamDescriptor& descriptor_;
	ParamDescriptor saved_;
};
}

Sound* LxrEditor::selectedSound() const {
	if (getRootUI() != &instrumentClipView || !getCurrentClip()) return nullptr;
	Sound* sound = nullptr;
	if (getCurrentOutputType() == OutputType::KIT) {
		if (getCurrentInstrumentClip()->affectEntire) return nullptr;
		Drum* drum = getCurrentKit()->selectedDrum;
		if (drum && drum->type == DrumType::SOUND) sound = static_cast<SoundDrum*>(drum);
	}
	else if (getCurrentOutputType() == OutputType::SYNTH) {
		sound = static_cast<SoundInstrument*>(getCurrentInstrument());
	}
	if (!sound || sound->getSynthMode() != SynthMode::SUBTRACTIVE
	    || view.activeModControllableModelStack.modControllable != sound) return nullptr;
	return sound;
}

Source* LxrEditor::selectedSource() {
	Sound* sound = selectedSound();
	if (!sound) return nullptr;
	if (!delxr::isLxr(sound->sources[source_].oscType)) source_ = 1 - source_;
	return delxr::isLxr(sound->sources[source_].oscType) ? &sound->sources[source_] : nullptr;
}

bool LxrEditor::tryOpen() {
	if (getCurrentUI() != &instrumentClipView || !isNoUIModeActive()) return false;
	source_ = 0;
	page_ = 0;
	if (!selectedSource()) return false;
	return openUI(this);
}

bool LxrEditor::opened() {
	refresh();
	return true;
}

void LxrEditor::rebuildPages() {
	pageCount_ = 0;
	Source* source = selectedSource();
	if (!source) return;
	const auto family = source->getLxrFamily();
	// Negative IDs -1..-4 are the four existing automatable engine controls.
	const Page candidates[] = {
	    {"PLAY 1", -1, -2}, {"PLAY 2", -3, -4},
	    {"OSC SHAPE", DELXR_LXR_CARRIER_WAVE, DELXR_LXR_PULSE_WIDTH},
	    {"OSC TUNE", DELXR_LXR_CARRIER_TUNE, DELXR_LXR_CARRIER_FINE},
	    {"FM1 SHAPE", DELXR_LXR_MOD1_WAVE, DELXR_LXR_MOD1_GAIN},
	    {"FM1 TUNE", DELXR_LXR_MOD1_TUNE, DELXR_LXR_MOD1_FINE},
	    {"FM ROUTING", DELXR_LXR_FM_MODE, none},
	    {"FM2 SHAPE", DELXR_LXR_MOD2_WAVE, DELXR_LXR_MOD2_GAIN},
	    {"FM2 TUNE", DELXR_LXR_MOD2_TUNE, DELXR_LXR_MOD2_FINE},
	    {"AMPLITUDE", DELXR_LXR_AMP_ATTACK, DELXR_LXR_AMP_SLOPE},
	    {"PITCH SHAPE", DELXR_LXR_PITCH_SLOPE, none},
	    {"PITCH MOD", DELXR_LXR_PITCH_AMOUNT, DELXR_LXR_PITCH_DECAY},
	    {"TRANSIENT", DELXR_LXR_TRANSIENT_WAVE, DELXR_LXR_TRANSIENT_LEVEL},
	    {"TRANSIENT TUNE", DELXR_LXR_TRANSIENT_TUNE, none},
	    {"NOISE", DELXR_LXR_NOISE_TUNE, none},
	    {"FILTER", DELXR_LXR_FILTER_TYPE, DELXR_LXR_FILTER_CUTOFF},
	    {"FILTER SHAPE", DELXR_LXR_FILTER_RESONANCE, DELXR_LXR_FILTER_DRIVE},
	    {"DIST / REPEAT", DELXR_LXR_DISTORTION, DELXR_LXR_REPEAT},
	};
	for (auto candidate : candidates) {
		if (candidate.top >= 0 && !delxr_lxr_patch_param_relevant(candidate.top, family)) candidate.top = none;
		if (candidate.bottom >= 0 && !delxr_lxr_patch_param_relevant(candidate.bottom, family)) candidate.bottom = none;
		if (candidate.top != none || candidate.bottom != none) pages_[pageCount_++] = candidate;
	}
	page_ = std::clamp(page_, 0, pageCount_ - 1);
}

int LxrEditor::parameterForKnob(int which) const {
	if (!pageCount_) return none;
	return which == 1 ? pages_[page_].top : pages_[page_].bottom;
}

int LxrEditor::primaryParamId(int control) const {
	static constexpr int ids[] = {params::LOCAL_OSC_A_PHASE_WIDTH, params::LOCAL_OSC_A_WAVE_INDEX,
	                              params::LOCAL_CARRIER_0_FEEDBACK, params::LOCAL_MODULATOR_0_FEEDBACK};
	return ids[control] + source_;
}

int LxrEditor::parameterValue(int parameter) {
	Source* source = selectedSource();
	if (!source || parameter == none) return 0;
	if (parameter < 0) {
		char memory[MODEL_STACK_MAX_SIZE];
		copyModelStack(memory, &view.activeModControllableModelStack, sizeof(ModelStackWithThreeMainThings));
		auto* stack = reinterpret_cast<ModelStackWithThreeMainThings*>(memory)->getPatchedAutoParamFromId(primaryParamId(-parameter - 1));
		if (!stack || !stack->autoParam) return 0;
		const int value = stack->autoParam->getValuePossiblyAtPos(view.modPos, stack);
		return stack->paramCollection->paramValueToKnobPos(value, stack) + 64;
	}
	DelxrLxrPatch defaults;
	delxr_lxr_patch_defaults(&defaults, source->getLxrFamily());
	return delxr_lxr_patch_get(source->getLxrPatch() ? source->getLxrPatch() : &defaults, parameter);
}

const char* LxrEditor::parameterLabel(int parameter) {
	if (parameter == none) return "--";
	if (parameter >= 0) return delxr_lxr_patch_param_label(parameter);
	static const char* const names[][4] = {
	    {"Decay", "Pitch amount", "Pitch decay", "Drive"},
	    {"Decay", "Noise cutoff", "Noise mix", "Clap"},
	    {"Decay", "FM master", "FM tune", "Filter cutoff"}
	};
	const auto family = selectedSource()->getLxrFamily();
	return names[family == DELXR_LXR_FAMILY_DRUM ? 0 : family == DELXR_LXR_FAMILY_SNARE ? 1 : 2][-parameter - 1];
}

void LxrEditor::parameterText(int parameter, char* text, int capacity) {
	if (parameter == none) { snprintf(text, capacity, "--"); return; }
	const int value = parameterValue(parameter);
	const char* name = parameter >= 0 ? delxr_lxr_patch_value_name(parameter, value) : nullptr;
	if (name) snprintf(text, capacity, "%s", name);
	else if (parameter == DELXR_LXR_CARRIER_FINE || parameter == DELXR_LXR_MOD1_FINE || parameter == DELXR_LXR_MOD2_FINE)
		snprintf(text, capacity, "%+d", value - 64);
	else snprintf(text, capacity, "%d", value);
}

void LxrEditor::refresh() {
	rebuildPages();
	display->cancelPopup();
	for (int knob = 0; knob < 2; ++knob) {
		const int parameter = parameterForKnob(knob);
		const int max = parameter >= 0 ? delxr_lxr_patch_param_max(parameter) : 128;
		indicator_leds::setKnobIndicatorLevel(knob, parameter == none ? 0 : parameterValue(parameter) * 128 / std::max(max, 1));
	}
	if (display->haveOLED()) renderUIsForOled();
	else display->setText(pageCount_ ? pages_[page_].title : "NO LXR");
}

ActionResult LxrEditor::horizontalEncoderAction(int32_t offset) {
	rebuildPages();
	if (pageCount_) page_ = (page_ + offset % pageCount_ + pageCount_) % pageCount_;
	refresh();
	return ActionResult::DEALT_WITH;
}

ActionResult LxrEditor::verticalEncoderAction(int32_t offset, bool inCardRoutine) {
	if (inCardRoutine) return ActionResult::REMIND_ME_OUTSIDE_CARD_ROUTINE;
	if (offset) source_ = 1 - source_;
	refresh();
	return ActionResult::DEALT_WITH;
}

void LxrEditor::modEncoderAction(int32_t which, int32_t offset) {
	Source* source = selectedSource();
	if (!source || which < 0 || which > 1) return;
	const int parameter = parameterForKnob(which);
	if (parameter == none) return;
	if (parameter < 0) {
		ScopedKnob binding(*selectedSound(), which, primaryParamId(-parameter - 1));
		view.modEncoderAction(which, offset);
	}
	else {
		const int oldValue = parameterValue(parameter);
		const int value = std::clamp<int32_t>(oldValue + offset, 0, delxr_lxr_patch_param_max(parameter));
		if (value != oldValue) {
			source->ensureLxrPatch()->value[parameter] = value;
			getCurrentInstrument()->beenEdited();
		}
	}
	refresh();
	if (display->have7SEG()) {
		char value[24];
		parameterText(parameter, value, sizeof(value));
		display->displayPopup(value);
	}
}

void LxrEditor::modEncoderButtonAction(uint8_t which, bool on) {
	if (!selectedSource() || which > 1) return;
	const int parameter = parameterForKnob(which);
	if (parameter < 0 && parameter != none
	    && (Buttons::isShiftButtonPressed() || Buttons::isButtonPressed(deluge::hid::button::LEARN))) {
		ScopedKnob binding(*selectedSound(), which, primaryParamId(-parameter - 1));
		instrumentClipView.modEncoderButtonAction(which, on);
	}
	else if (on && parameter >= 0 && (Buttons::isShiftButtonPressed() || Buttons::isButtonPressed(deluge::hid::button::LEARN))) {
		display->displayPopup("Live patch control");
	}
}

void LxrEditor::modButtonAction(uint8_t which, bool on) {
	// The original page buttons become direct section shortcuts in this editor.
	if (!on) return;
	if (which == 0 && Buttons::isShiftButtonPressed()) { exitUI(); return; }
	rebuildPages();
	if (pageCount_) page_ = std::min<int>(which, pageCount_ - 1);
	refresh();
}

ActionResult LxrEditor::padAction(int32_t x, int32_t y, int32_t velocity) {
	// Keep live auditions, mutes and note entry; do not open another sound editor
	// via Shift/grid or audition/grid shortcuts over this one.
	if (velocity && x < kDisplayWidth && (Buttons::isShiftButtonPressed() || isUIModeActive(UI_MODE_AUDITIONING)))
		return ActionResult::DEALT_WITH;
	const auto result = instrumentClipView.padAction(x, y, velocity);
	if (getCurrentUI() == this) refresh();
	return result;
}

ActionResult LxrEditor::buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) {
	using namespace deluge::hid::button;
	if (b == BACK) {
		if (inCardRoutine) return ActionResult::REMIND_ME_OUTSIDE_CARD_ROUTINE;
		if (on) exitUI();
		return ActionResult::DEALT_WITH;
	}
	// Transport and Shift retain the firmware's global handling. Select and the
	// right-side tempo encoder are never used for sound-parameter editing.
	if (b == PLAY || b == RECORD || b == SHIFT || b == TEMPO_ENC || b == TAP_TEMPO
	    || b == MOD_ENCODER_0 || b == MOD_ENCODER_1)
		return ActionResult::NOT_DEALT_WITH;
	return ActionResult::DEALT_WITH;
}

ActionResult LxrEditor::exitUI() {
	close();
	view.setKnobIndicatorLevels();
	return ActionResult::DEALT_WITH;
}

void LxrEditor::renderOLED(deluge::hid::display::oled_canvas::Canvas& canvas) {
	Source* source = selectedSource();
	if (!source || !pageCount_) {
		canvas.drawStringCentred("Select an LXR row", 16, 6, 7);
		canvas.drawStringCentred("BACK: exit", 33, 6, 7);
		return;
	}
	static const char* const families[] = {"DRUM", "SNARE", "CYMBAL", "CLOSED HAT", "OPEN HAT"};
	char title[40];
	snprintf(title, sizeof(title), "LXR %s  OSC%d", families[source->getLxrFamily()], source_ + 1);
	canvas.drawString(title, 0, 0, 6, 7);
	snprintf(title, sizeof(title), "%s  %d/%d", pages_[page_].title, page_ + 1, pageCount_);
	canvas.drawString(title, 0, 9, 6, 7);
	canvas.drawHorizontalLine(17, 0, 127);
	for (int row = 0; row < 2; ++row) {
		const int parameter = row == 0 ? pages_[page_].top : pages_[page_].bottom;
		const int y = 20 + row * 11;
		char value[24];
		parameterText(parameter, value, sizeof(value));
		canvas.drawString(parameterLabel(parameter), 0, y, 5, 7);
		canvas.drawStringAlignRight(value, y, 6, 7);
	}
	canvas.drawString(page_ < 2 ? "AUTO  X:page Y:osc" : "LIVE  X:page Y:osc", 0, 41, 6, 7);
}
