#include "plugin.hpp"

struct Pythagoras : Module {
	enum ParamIds {
		TRIM_1_PARAM,
		DENOMINATOR_1_PARAM,
		NUMERATOR_1_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		INPUT_1_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_VOCT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	midi::InputQueue midiInput;

	Pythagoras() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TRIM_1_PARAM, -1.f, 1.f, 0.5f, "Trim");
		configParam(DENOMINATOR_1_PARAM, 0.f, 10.f, 3.f, "Denominator");
		configParam(NUMERATOR_1_PARAM, 0.f, 10.f, 4.f, "Numerator");
	}

	void process(const ProcessArgs &args) override {
		midi::Message msg;
		while (midiInput.shift(&msg)) {
			processMidiMessage(msg);
		}
	};

	// Adapted from core::MIDI_CV
	void processMidiMessage(midi::Message msg) {
		DEBUG("MIDI: %01x %01x %02x %02x", msg.getStatus(), msg.getChannel(), msg.getNote(), msg.getValue());
		switch (msg.getStatus()) {

			case 0x8: {
				// note off
				// releaseNote(msg.getNote());
			} break;

			case 0x9: {
				// note on
				if (msg.getValue() > 0) {
					// int c = msg.getChannel();
					// velocities[c] = msg.getValue();
				} else {
					// Some keyboards send a "note on" event with a velocity of 0 to signal that the key has been released.
				}
			} break;

			case 0xa: {
				// Aftertouch pressure
			} break;

			case 0xb: {
				// Continuous controller
				// processCC(msg);
			} break;

			case 0xe: {
				// Pitch bend
				// pitches[c] = ((uint16_t) msg.getValue() << 7) | msg.getNote();
			} break;

			case 0xf: {
				// Sysex (for timing pulse and start/stop)
			} break;

			default: break;
		}
	}

};

struct PythagorasWidget : ModuleWidget {
	PythagorasWidget(Pythagoras* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Pythagoras.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<RoundBlackKnob>(mm2px(Vec(3.195, 41.321)), module, Pythagoras::TRIM_1_PARAM));
		addParam(createParam<RoundBlackSnapKnob>(mm2px(Vec(3.195, 53.877)), module, Pythagoras::DENOMINATOR_1_PARAM));
		addParam(createParam<RoundBlackSnapKnob>(mm2px(Vec(3.195, 66.432)), module, Pythagoras::NUMERATOR_1_PARAM));

		addInput(createInput<PJ301MPort>(mm2px(Vec(60, 12)), module, Pythagoras::INPUT_1_INPUT));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(60, 24)), module, Pythagoras::OUT_VOCT_OUTPUT));

		MidiWidget* midiWidget = createWidget<MidiWidget>(mm2px(Vec(3.41891, 14.8373)));
		midiWidget->box.size = mm2px(Vec(55, 28));
		midiWidget->setMidiPort(module ? &(module->midiInput) : NULL); // without null check, segfaults when showing module library
		NVGcolor color = nvgRGB(0x50, 0x80, 0xff);
		midiWidget->driverChoice->color = color;
		midiWidget->deviceChoice->color = color;
		midiWidget->channelChoice->color = color;
		addChild(midiWidget);
	}
};


Model* modelPythagoras = createModel<Pythagoras, PythagorasWidget>("Pythagoras");