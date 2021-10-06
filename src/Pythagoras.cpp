#include "plugin.hpp"

#define N_NOTES 6

struct Pythagoras : Module {
	enum ParamIds {
		ENUMS(ROOT_NOTE_PARAM, N_NOTES),
		ENUMS(NUMERATOR_PARAM, N_NOTES),
		ENUMS(DENOMINATOR_PARAM, N_NOTES),
		ENUMS(TRIM_PARAM, N_NOTES),
		NUM_PARAMS
	};
	enum InputIds {
		INPUT_1_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		PITCH_OUTPUT,
		GATE_OUTPUT,
		VELOCITY_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	midi::InputQueue midiInput;

	Pythagoras() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i = 0; i < N_NOTES; ++i) {
			configParam(ROOT_NOTE_PARAM + i, 0.f, N_NOTES, 0.f, "Root note");
			configParam(NUMERATOR_PARAM + i, 1.f, 12.f, 4.f, "Numerator");
			configParam(DENOMINATOR_PARAM + i, 1.f, 12.f, 3.f, "Denominator");
			configParam(TRIM_PARAM + i, -0.1f, 0.1f, 0.0f, "Trim");
		}
		configOutput(PITCH_OUTPUT, "Pitch (1V/oct)");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(VELOCITY_OUTPUT, "Velocity");
	}

	void process(const ProcessArgs &args) override {
		midi::Message msg;
		while (midiInput.tryPop(&msg, args.frame)) {
			processMidiMessage(msg);
		}
	};

	// bool gate;

	// Handle the fact that multiple keys may be pressed and released in arbitrary order.
	int lastKeyDown = 0;

	// Map each accidental key to the same scale degree as the natural key immediately to its left
	int whiteKey[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};

	float getVoltageForScaleDegree (int noteInOctave) {
		// One volt per octave - root note (do / C) of octave N is N volts.
		float pitchVoltage = 0;
		if (noteInOctave > 0 && noteInOctave <= N_NOTES) {
			int p = noteInOctave - 1;
			float numerator = params[NUMERATOR_PARAM + p].getValue();
			float denominator = params[DENOMINATOR_PARAM + p].getValue();
			// TODO check that inputs and fraction are in expected range: finite, positive, ends up in 0...1
			float fraction = numerator / denominator;
			if (!(isfinite(fraction) || fraction > 0)) {
				DEBUG("Fraction out of range.");
				return 0;
			}
			while (fraction >= 2) fraction /= 2;
			while (fraction < 1) fraction *= 2;
			// In domain 1...2 log2 has range 0...1
			pitchVoltage += log2(fraction) + params[TRIM_PARAM + p].getValue();
			DEBUG("frac, log(frac), pitchv: %0.2f %0.2f %0.2f", fraction, log2(fraction), pitchVoltage);
		}
		return pitchVoltage;
	}

	// Scale to one volt per octave
	// MIDI note in range 0-127 inclusive.
	float getVoltageForMidiNote (int note) {
		// Find the frequency (potentially in another octave) as a multiple of the octave root note, then
		// repeatedly divide or multiply that frequency by two to shift into current octave, in range [1...2).
		// Note in octave is chromatic - includes naturals and accidentals.
		int octave = note / 12;
		int noteInOctave = note - (octave * 12);
		int scaleDegree = whiteKey[noteInOctave];
		return octave + getVoltageForScaleDegree(scaleDegree);
	}

	void noteOn(int note, int velocity) {
		outputs[PITCH_OUTPUT].setVoltage(getVoltageForMidiNote(note), 0);
		outputs[GATE_OUTPUT].setVoltage(10.f, 0);
		outputs[VELOCITY_OUTPUT].setVoltage(rescale(velocity, 0, 127, 0.f, 10.f), 0);
		lastKeyDown = note;
	}

	void noteOff(int note) {
		outputs[VELOCITY_OUTPUT].setVoltage(0, 0);
		if (note == lastKeyDown) {
			outputs[GATE_OUTPUT].setVoltage(0, 0);
		}
	}

	void processMidiMessage(midi::Message msg) {
		DEBUG("Status/channel/note/velocity: %01x %01x %02x %02x", msg.getStatus(), msg.getChannel(), msg.getNote(),
			  msg.getValue());
		switch (msg.getStatus()) {
			case 0x8: {
				// Note Off message
				noteOff(0);
			}
				break;

			case 0x9: {
				// Note On message
				// actually we need to keep one gate per note - try playing several keys legato, releasing the last one.
				int note = msg.getNote();
				int velocity = msg.getValue();
				if (velocity == 0) {
					// Many keyboards send a "note on" status with a velocity of 0 to signal that the key has been
					// released. This is allowed by the MIDI spec and useful for something called "running status".
					noteOff(note);
				} else {
					noteOn(note, velocity);
				}
			}
				break;

				// Other cases: 0xa Aftertouch pressure, 0xb Continuous controller, 0xe Pitch bend, 0xf Sysex (timing and start/stop)
			default:
				break;
		}
	}

	// Groups together all parameters for one note of the scale. A temporary buffer while sorting and copying.
	// We could use these at all times rather than just during sorting to memoize results and reduce computation.
	// They could have methods to read/write from params at the same index, and check whether any params had changed.
	// It may also be possible to register onChange events on the knobs.
	struct NoteParams {
		float voltage; // voltage of this note in octave zero
		float numerator;
		float denominator;
		float trim;
	};

	static int noteParamsCompare(const void *a, const void *b) {
		float va = ((struct NoteParams *) a)->voltage;
		float vb = ((struct NoteParams *) b)->voltage;
		// Compare floating point values returning int. https://stackoverflow.com/a/3886497
		return (va > vb) - (va < vb);
	}

	// Arrange scale notes in order of increasing frequency, setting knobs accordingly.
	void sortNotes() {
		DEBUG("Sorting notes...");
		struct NoteParams n[N_NOTES];
		// CLEARING OUT RANDOMIZED TRIM
		for (int i = 0; i < N_NOTES; ++i) {
			params[TRIM_PARAM + i].setValue(0);
		}
		for (int i = 0; i < N_NOTES; ++i) {
			n[i].voltage = getVoltageForScaleDegree(i + 1);
			n[i].numerator = params[NUMERATOR_PARAM + i].getValue();
			n[i].denominator = params[DENOMINATOR_PARAM + i].getValue();
			n[i].trim = params[TRIM_PARAM + i].getValue();
		}
		qsort(n, N_NOTES, sizeof(struct NoteParams), noteParamsCompare);
		for (int i = 0; i < N_NOTES; ++i) {
			DEBUG("Voltage %0.2f", n[i].voltage);
			params[NUMERATOR_PARAM + i].setValue(n[i].numerator);
			params[DENOMINATOR_PARAM + i].setValue(n[i].denominator);
			params[TRIM_PARAM + i].setValue(n[i].trim);
		}
	};
};

struct PythagorasWidget : ModuleWidget {

		LedDisplay *midiDisplay;
		LedDisplayChoice *midiMessage;

		PythagorasWidget(Pythagoras *module) {
			setModule(module);
			setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Pythagoras.svg")));

			addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
			addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
			addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
			addChild(createWidget<ScrewSilver>(
					Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

			// Knobs are continuous, SnapKnobs are quantized to integers.
			for (int i = 0; i < N_NOTES; ++i) {
				int x = 6 + 12 * i;
				addParam(createParam<RoundBlackSnapKnob>(mm2px(Vec(x, 55 + 12 * 0)), module,
														 Pythagoras::ROOT_NOTE_PARAM + i));
				addParam(createParam<RoundBlackSnapKnob>(mm2px(Vec(x, 55 + 12 * 1)), module,
														 Pythagoras::NUMERATOR_PARAM + i));
				addParam(createParam<RoundBlackSnapKnob>(mm2px(Vec(x, 55 + 12 * 2)), module,
														 Pythagoras::DENOMINATOR_PARAM + i));
				addParam(createParam<RoundBlackKnob>(mm2px(Vec(x, 55 + 12 * 3)), module, Pythagoras::TRIM_PARAM + i));
			}

			addInput(createInput<PJ301MPort>(mm2px(Vec(60, 12)), module, Pythagoras::INPUT_1_INPUT));

			addOutput(createOutput<PJ301MPort>(mm2px(Vec(60, 24)), module, Pythagoras::PITCH_OUTPUT));
			addOutput(createOutput<PJ301MPort>(mm2px(Vec(60, 36)), module, Pythagoras::GATE_OUTPUT));

			// MIDI input and channel selector
			MidiWidget *midiWidget = createWidget<MidiWidget>(mm2px(Vec(3.41891, 14.8373)));
			midiWidget->box.size = mm2px(Vec(55, 28));
			midiWidget->setMidiPort(
					module ? &(module->midiInput) : NULL); // without null check, segfaults when showing module library
			NVGcolor color = nvgRGB(0x50, 0x80, 0xff);
			midiWidget->driverChoice->color = color;
			midiWidget->deviceChoice->color = color;
			midiWidget->channelChoice->color = color;
			addChild(midiWidget);

			// MIDI input debug display (with resulting v/oct)
			// LedDisplayTextField is an editable field, as in a "patch notes" module.
			// LedDisplayChoice is just the text with no background. You need to add it to an LedDisplay for the full effect.
			// maybe set a pointer field on the module to correspond to the midiMessage object so we can set its text? so bad.
			midiDisplay = createWidget<LedDisplay>(mm2px(Vec(3, 105)));
			midiDisplay->box.size = mm2px(Vec(75, 10));
			midiMessage = createWidget<LedDisplayChoice>(mm2px(Vec(0, 0)));
			midiMessage->box.size = midiDisplay->box.size;
			midiMessage->color = color;
			midiMessage->text = std::string("MIDI Message display");
			midiDisplay->addChild(midiMessage);
			addChild(midiDisplay);

		}

		void appendContextMenu(Menu* menu) override {
			// Pythagoras *module = dynamic_cast<Pythagoras *>(this->module);
			menu->addChild(new MenuSeparator);
			menu->addChild(createMenuItem("Sort notes within octave", "",
					// Knobs refresh automatically when parameters are set.
										  [=]() {((Pythagoras*) module)->sortNotes();}));
		}

		// void randomizeAction () override { // how do we customize this?
	};

Model* modelPythagoras = createModel<Pythagoras, PythagorasWidget>("Pythagoras");
