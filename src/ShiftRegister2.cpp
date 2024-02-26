#include "ML_modules.hpp"



#include <cmath>
#include <cstdlib>

#define MAX(a,b) a>b?a:b

struct ShiftRegister2 : Module {
	enum ParamIds {
		NUM_STEPS_PARAM,
		PROB1_PARAM,
		PROB2_PARAM,
		MIX1_PARAM,
		AUX_OFFSET_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		IN1_INPUT,
		IN2_INPUT,
		TRIGGER_INPUT,
		NUM_STEPS_INPUT,
		PROB1_INPUT,
		PROB2_INPUT,
		MIX1_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		AUX_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	ShiftRegister2() { 
		config( NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS ); 
		configParam(ShiftRegister2::NUM_STEPS_PARAM, 1.0, 16.0, 8.0);
		configParam(ShiftRegister2::PROB1_PARAM, 0.0, 1.0, 0.0);
		configParam(ShiftRegister2::PROB2_PARAM, 0.0, 1.0, 0.0);
		configParam(ShiftRegister2::MIX1_PARAM, 0.0, 1.0, 1.0);
		configParam(ShiftRegister2::AUX_OFFSET_PARAM, 1.0, 16.0, 1.0);
		onReset(); 
	};



	void process(const ProcessArgs &args) override;

	int numSteps = 0;
	float values[32 * PORT_MAX_CHANNELS] = {};
	int channels[32] = {};

	dsp::SchmittTrigger trigTrigger[PORT_MAX_CHANNELS];

	inline float randf() {return rand()/(RAND_MAX-1.0f);}

	void onReset() override {
		memset(channels, 0, 32*sizeof(int));
		memset(values, 0, 32*PORT_MAX_CHANNELS*sizeof(float));
	};

};



void ShiftRegister2::process(const ProcessArgs &args) {


	numSteps = roundf(clamp(params[NUM_STEPS_PARAM].getValue() * clamp(inputs[NUM_STEPS_INPUT].getNormalVoltage(5.0f),0.0f,5.0f)/5.0f,1.0f,16.0f));


	if( inputs[TRIGGER_INPUT].isConnected() ) {

		int trig_channels = inputs[TRIGGER_INPUT].getChannels();
	
		if(trig_channels==1) {

			if( trigTrigger[0].process(inputs[TRIGGER_INPUT].getVoltage()) ) {

				int in1_channels = inputs[IN1_INPUT].getChannels();
				int in2_channels = inputs[IN2_INPUT].getChannels();
				int in_channels = MAX(in1_channels, in2_channels);
				in_channels = MAX(1, in_channels);
	
				for(int i=31; i>0; i--) {
					memcpy(values + i*PORT_MAX_CHANNELS, values + (i-1)*PORT_MAX_CHANNELS, PORT_MAX_CHANNELS * sizeof(float));
					channels[i] = channels[i-1];
				}

				channels[0] = in_channels;

				for(int c=0; c<in_channels; c++) {

	 				float new_in1 = inputs[IN1_INPUT].getNormalPolyVoltage( randf()*10.0-5.0, c );
					float new_in2 = inputs[IN2_INPUT].getNormalPolyVoltage( new_in1 + 1.0,    c );
	
					float p1 = params[PROB1_PARAM].getValue() + clamp(inputs[PROB1_INPUT].getNormalPolyVoltage(0.0f, c),-10.0f,10.0f)/10.0f;
					float p2 = params[PROB2_PARAM].getValue() + clamp(inputs[PROB2_INPUT].getNormalPolyVoltage(0.0f, c),-10.0f,10.0f)/10.0f;

					bool replace = ( randf() < p1 );
					bool rnd2 = ( randf() < p2 );

					float a = params[MIX1_PARAM].getValue() + clamp(inputs[MIX1_INPUT].getNormalPolyVoltage(0.0f, c),-10.0f,10.0f)/10.0f;

					if(replace) values[c] = a* (rnd2?new_in2:new_in1) + (1-a)*values[numSteps*PORT_MAX_CHANNELS + c];
					else				values[c] = values[numSteps*PORT_MAX_CHANNELS + c];
				}

			}

		} else {

			bool any_trig = false;

			for(int c=0; c<trig_channels; c++) {

				if( trigTrigger[c].process(inputs[TRIGGER_INPUT].getPolyVoltage(c)) ) {

					any_trig = true;

					for(int i=31; i>0; i--) values[i*PORT_MAX_CHANNELS + c] = values[(i-1)*PORT_MAX_CHANNELS + c];

		 			float new_in1 = inputs[IN1_INPUT].getNormalPolyVoltage( randf()*10.0f-5.0f, c );
					float new_in2 = inputs[IN2_INPUT].getNormalPolyVoltage( new_in1 + 1.0f,    c );
	
					float p1 = params[PROB1_PARAM].getValue() + clamp(inputs[PROB1_INPUT].getNormalPolyVoltage(0.0f, c),-10.0f,10.0f)/10.0f;
					float p2 = params[PROB2_PARAM].getValue() + clamp(inputs[PROB2_INPUT].getNormalPolyVoltage(0.0f, c),-10.0f,10.0f)/10.0f;

					bool replace = ( randf() < p1 );
					bool rnd2 = ( randf() < p2 );

					float a = params[MIX1_PARAM].getValue() + clamp(inputs[MIX1_INPUT].getNormalPolyVoltage(0.0f, c),-10.0f,10.0f)/10.0f;

					if(replace) values[c] = a* (rnd2?new_in2:new_in1) + (1-a)*values[numSteps*PORT_MAX_CHANNELS + c];
					else				values[c] = values[numSteps*PORT_MAX_CHANNELS + c];

				}
			}
			
			if(any_trig) {

				int in1_channels = inputs[IN1_INPUT].getChannels();
				int in2_channels = inputs[IN2_INPUT].getChannels();
				int in_channels = MAX(in1_channels, in2_channels);
				in_channels = MAX(in_channels, trig_channels);

				for(int i=31; i>0; i--) {	
					channels[i] = channels[i-1];	
				}
				channels[0] = in_channels;
			}

		};

	};

	outputs[OUT_OUTPUT].setChannels(channels[0]);
	outputs[OUT_OUTPUT].writeVoltages(values);

	int offset = roundf(params[AUX_OFFSET_PARAM].getValue());

	outputs[AUX_OUTPUT].setChannels(channels[offset]);
	outputs[AUX_OUTPUT].writeVoltages(values + offset*PORT_MAX_CHANNELS);
};

struct IntDisplayWidget : TransparentWidget {

  int *value=0;
  std::shared_ptr<Font> font;

  IntDisplayWidget() {
    font = APP->window->loadFont(asset::plugin(pluginInstance, "res/Segment7Standard.ttf"));
  };

  void draw(const DrawArgs &args) override {
    // Background
    NVGcolor backgroundColor = nvgRGB(0x20, 0x20, 0x20);
    NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
    nvgFillColor(args.vg, backgroundColor);
    nvgFill(args.vg);
    nvgStrokeWidth(args.vg, 1.0);
    nvgStrokeColor(args.vg, borderColor);
    nvgStroke(args.vg);

    nvgFontSize(args.vg, 18);
    nvgFontFaceId(args.vg, font->handle);
    nvgTextLetterSpacing(args.vg, 2.5);

//    std::string to_display = std::to_string( (unsigned) *value);

    char displayStr[3];
//    while(to_display.length()<1) to_display = ' ' + to_display;

    if(value) {snprintf(displayStr, sizeof(displayStr), "%2u", (unsigned) *value);}

    Vec textPos = Vec(6.0f, 17.0f);

    NVGcolor textColor = nvgRGB(0xdf, 0xd2, 0x2c);
    nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
    nvgText(args.vg, textPos.x, textPos.y, "~~", NULL);

    textColor = nvgRGB(0xda, 0xe9, 0x29);
    nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
    nvgText(args.vg, textPos.x, textPos.y, "\\\\", NULL);

    textColor = nvgRGB(0xf0, 0x00, 0x00);
    nvgFillColor(args.vg, textColor);
    nvgText(args.vg, textPos.x, textPos.y, displayStr,  NULL);
  }
};


struct ShiftRegister2Widget : ModuleWidget {
	ShiftRegister2Widget(ShiftRegister2 *module);
};

ShiftRegister2Widget::ShiftRegister2Widget(ShiftRegister2 *module) {
		
	setModule(module);

	box.size = Vec(15*8, 380);

	{
		SvgPanel *panel = new SvgPanel();
		panel->box.size = box.size;
		panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance,"res/ShiftReg2.svg")));

		addChild(panel);
	}

	addChild(createWidget<MLScrew>(Vec(15, 0)));
	addChild(createWidget<MLScrew>(Vec(box.size.x-30, 0)));
	addChild(createWidget<MLScrew>(Vec(15, 365)));
	addChild(createWidget<MLScrew>(Vec(box.size.x-30, 365)));




	const float column1 = 19, column2 = 74;


	IntDisplayWidget *display = new IntDisplayWidget();
	display->box.pos = Vec(65,46);
	display->box.size = Vec(40, 20);
	if(module) display->value = &(module->numSteps);
	addChild(display);

	addInput(createInput<MLPort>(Vec(column1,  44), module, ShiftRegister2::TRIGGER_INPUT));
	addInput(createInput<MLPort>(Vec(column1, 96), module, ShiftRegister2::NUM_STEPS_INPUT));
        
	addParam(createParam<RedSnapMLKnob>(Vec(65,  86), module, ShiftRegister2::NUM_STEPS_PARAM));

	addInput(createInput<MLPort>(Vec(column1+8,  135), module, ShiftRegister2::IN1_INPUT));
	addInput(createInput<MLPort>(Vec(column2-8,  135), module, ShiftRegister2::IN2_INPUT));


	addInput(createInput<MLPort>(Vec(column1+3,  183), module, ShiftRegister2::PROB1_INPUT));
  addParam(createParam<SmallBlueMLKnob>(Vec(column2, 176), module, ShiftRegister2::PROB1_PARAM));

	addInput(createInput<MLPort>(Vec(column1+3,  229), module, ShiftRegister2::PROB2_INPUT));
  addParam(createParam<SmallBlueMLKnob>(Vec(column2, 222), module, ShiftRegister2::PROB2_PARAM));
	
	addInput(createInput<MLPort>(Vec(column1+3,  275), module, ShiftRegister2::MIX1_INPUT));
	addParam(createParam<SmallBlueMLKnob>(Vec(column2,  268), module, ShiftRegister2::MIX1_PARAM));
  
	addParam(createParam<Trimpot>(Vec(56,  318), module, ShiftRegister2::AUX_OFFSET_PARAM));

	addOutput(createOutput<MLPort>(Vec(column1-2, 328 ), module, ShiftRegister2::OUT_OUTPUT));
	addOutput(createOutput<MLPort>(Vec(column2+2, 328 ), module, ShiftRegister2::AUX_OUTPUT));
}


Model *modelShiftRegister2 = createModel<ShiftRegister2, ShiftRegister2Widget>("Evolution");
