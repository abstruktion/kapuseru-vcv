#include "plugin.hpp"
#include <queue>

struct Superflat : Module {
	enum ParamId {
		BUTTON_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Superflat() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(BUTTON_PARAM, "Flatten");
	}

	void process(const ProcessArgs& args) override {
	}
};

struct SuperFlatSwitch : CKD6 {

	// Start true. When button appears its state will change and flip to unflat.
	bool superflat = true;

	SuperFlatSwitch() {
		momentary = false;
		CKD6();
	}

	// Will not apply to new modules. TODO re-apply occasionally.
	void onChange(const ChangeEvent& e) {

		// TODO just use param value
		superflat = !superflat;
		bool visible = !superflat;
		std::queue<Widget *> q;
		Widget *rack = APP->scene->rack;
		q.push(rack);
		while (!q.empty()) {
			Widget *w = q.front();
			q.pop();
			for (Widget *w1: w->children) {
				q.push(w1);
			}
			if (dynamic_cast<SvgScrew *>(w) || dynamic_cast<RailWidget *>(w) || dynamic_cast<CircularShadow *>(w)) {
				w->setVisible(visible);
			}
			// Shadows do not disappear until you move the knob. Mark knob framebuffers dirty to redraw them.
			SvgKnob *svgKnob = dynamic_cast<SvgKnob *>(w);
			if (svgKnob) {
				svgKnob->fb->dirty = true;
			}
		}
		CKD6::onChange(e);
	}

};


struct SuperflatWidget : ModuleWidget {
	SuperflatWidget(Superflat* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Superflat.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<SuperFlatSwitch>(Vec(8, 60), module, Superflat::BUTTON_PARAM));
	}


};

Model* modelSuperflat = createModel<Superflat, SuperflatWidget>("Superflat");