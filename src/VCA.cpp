#include "plugin.hpp"


struct VCA : Module {
	enum ParamIds {
		LEVEL1_PARAM,
		LEVEL2_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		EXP1_INPUT,
		LIN1_INPUT,
		IN1_INPUT,
		EXP2_INPUT,
		LIN2_INPUT,
		IN2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT1_OUTPUT,
		OUT2_OUTPUT,
		NUM_OUTPUTS
	};

	VCA() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
		configParam(LEVEL1_PARAM, 0.0, 1.0, 1.0, "Ch 1 level", "%", 0, 100);
		configParam(LEVEL2_PARAM, 0.0, 1.0, 1.0, "Ch 2 level", "%", 0, 100);
	}

	void processChannel(Input &in, Param &level, Input &lin, Input &exp, Output &out) {
		// Get input
		int channels = in.getChannels();
		simd::float_4 v[4];
		for (int c = 0; c < channels; c += 4) {
			v[c / 4] = simd::float_4::load(in.getVoltages(c));
		}

		// Apply knob gain
		float gain = level.getValue();
		for (int c = 0; c < channels; c += 4) {
			v[c / 4] *= gain;
		}

		// Apply linear CV gain
		if (lin.isConnected()) {
			if (lin.isPolyphonic()) {
				for (int c = 0; c < channels; c += 4) {
					simd::float_4 cv = simd::float_4::load(lin.getVoltages(c)) / 10.f;
					cv = clamp(cv, 0.f, 1.f);
					v[c / 4] *= cv;
				}
			}
			else {
				float cv = lin.getVoltage() / 10.f;
				cv = clamp(cv, 0.f, 1.f);
				for (int c = 0; c < channels; c += 4) {
					v[c / 4] *= cv;
				}
			}
		}

		// Apply exponential CV gain
		const float expBase = 50.f;
		if (exp.isConnected()) {
			if (exp.isPolyphonic()) {
				for (int c = 0; c < channels; c += 4) {
					simd::float_4 cv = simd::float_4::load(exp.getVoltages(c)) / 10.f;
					cv = clamp(cv, 0.f, 1.f);
					cv = rescale(pow(expBase, cv), 1.f, expBase, 0.f, 1.f);
					v[c / 4] *= cv;
				}
			}
			else {
				float cv = exp.getVoltage() / 10.f;
				cv = clamp(cv, 0.f, 1.f);
				cv = rescale(std::pow(expBase, cv), 1.f, expBase, 0.f, 1.f);
				for (int c = 0; c < channels; c += 4) {
					v[c / 4] *= cv;
				}
			}
		}

		// Set output
		out.setChannels(channels);
		for (int c = 0; c < channels; c += 4) {
			v[c / 4].store(out.getVoltages(c));
		}
	}

	void process(const ProcessArgs &args) override {
		processChannel(inputs[IN1_INPUT], params[LEVEL1_PARAM], inputs[LIN1_INPUT], inputs[EXP1_INPUT], outputs[OUT1_OUTPUT]);
		processChannel(inputs[IN2_INPUT], params[LEVEL2_PARAM], inputs[LIN2_INPUT], inputs[EXP2_INPUT], outputs[OUT2_OUTPUT]);
	}
};



struct VCAWidget : ModuleWidget {
	VCAWidget(VCA *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/VCA.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParam<RoundLargeBlackKnob>(mm2px(Vec(6.35, 19.11753)), module, VCA::LEVEL1_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(mm2px(Vec(6.35, 74.80544)), module, VCA::LEVEL2_PARAM));

		addInput(createInput<PJ301MPort>(mm2px(Vec(2.5907, 38.19371)), module, VCA::EXP1_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(14.59752, 38.19371)), module, VCA::LIN1_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(2.5907, 52.80642)), module, VCA::IN1_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(2.5907, 93.53435)), module, VCA::EXP2_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(14.59752, 93.53435)), module, VCA::LIN2_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(2.5907, 108.14706)), module, VCA::IN2_INPUT));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(14.59752, 52.80642)), module, VCA::OUT1_OUTPUT));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(14.59752, 108.14706)), module, VCA::OUT2_OUTPUT));
	}
};


Model *modelVCA = createModel<VCA, VCAWidget>("VCA");


struct VCA_1 : Module {
	enum ParamIds {
		LEVEL_PARAM,
		EXP_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CV_INPUT,
		IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	float amplitude = 0.f;

	VCA_1() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(LEVEL_PARAM, 0.0, 1.0, 1.0, "Level", "%", 0, 100);
		configParam(EXP_PARAM, 0.0, 1.0, 1.0, "Response mode");
	}

	void process(const ProcessArgs &args) override {
		int channels = inputs[IN_INPUT].getChannels();
		float amplitudeSum = 0.f;
		float level = params[LEVEL_PARAM].getValue();

		for (int c = 0; c < channels; c++) {
			// Get input
			float in = inputs[IN_INPUT].getVoltage(c);

			// Get gain
			float gain = level;
			if (inputs[CV_INPUT].isConnected()) {
				float cv = clamp(inputs[CV_INPUT].getPolyVoltage(c) / 10.f, 0.f, 1.f);
				if ((int) std::round(params[EXP_PARAM].getValue()) == 0)
					cv = std::pow(cv, 4.f);
				gain *= cv;
			}

			// Apply gain
			in *= gain;
			amplitudeSum += gain;

			// Set output
			outputs[OUT_OUTPUT].setVoltage(in, c);
		}

		outputs[OUT_OUTPUT].setChannels(channels);
		amplitude = (channels > 0) ? (amplitudeSum / channels) : 0.f;
	}
};


struct VCA_1VUKnob : SliderKnob {
	VCA_1 *module = NULL;

	VCA_1VUKnob() {
		box.size = mm2px(Vec(10, 46));
	}

	void draw(const DrawArgs &args) override {
		float amplitude = module ? module->amplitude : 1.f;

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.0);
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgFill(args.vg);

		const int segs = 25;
		const Vec margin = Vec(3, 3);
		Rect r = box.zeroPos().grow(margin.neg());

		for (int i = 0; i < segs; i++) {
			float value = paramQuantity ? paramQuantity->getValue() : 1.f;
			float segValue = clamp(value * segs - (segs - i - 1), 0.f, 1.f);
			float segAmplitude = clamp(amplitude * segs - (segs - i - 1), 0.f, 1.f);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, r.pos.x, r.pos.y + r.size.y / segs * i + 0.5,
				r.size.x, r.size.y / segs - 1.0);
			if (segValue > 0.f) {
				nvgFillColor(args.vg, color::alpha(nvgRGBf(0.33, 0.33, 0.33), segValue));
				nvgFill(args.vg);
			}
			if (segAmplitude > 0.f) {
				nvgFillColor(args.vg, color::alpha(SCHEME_GREEN, segAmplitude));
				nvgFill(args.vg);
			}
		}
	}
};


struct VCA_1Widget : ModuleWidget {
	VCA_1Widget(VCA_1 *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/VCA-1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		VCA_1VUKnob *levelParam = createParam<VCA_1VUKnob>(mm2px(Vec(2.62103, 12.31692)), module, VCA_1::LEVEL_PARAM);
		levelParam->module = module;
		addParam(levelParam);
		addParam(createParam<CKSS>(mm2px(Vec(5.24619, 79.9593)), module, VCA_1::EXP_PARAM));

		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51261, 60.4008)), module, VCA_1::CV_INPUT));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.51398, 97.74977)), module, VCA_1::IN_INPUT));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.51398, 108.64454)), module, VCA_1::OUT_OUTPUT));
	}
};


Model *modelVCA_1 = createModel<VCA_1, VCA_1Widget>("VCA-1");
