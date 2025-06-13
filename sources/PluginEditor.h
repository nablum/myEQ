/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

enum FFTOrder
{
    order2048 = 11,
    order4096 = 12,
    order8192 = 13
};

template<typename BlockType>
struct FFTDataGenerator
{
    /**
        Produces the FFT data from an audio buffer
     */
    void produceFFTDataForRendering(const juce::AudioBuffer<float>& audioData, const float negativeInfinity)
    {
        const auto fftSize = getFFTSize();
        
        fftData.assign(fftData.size(), 0);
        auto* readIndex = audioData.getReadPointer(0);
        std::copy(readIndex, readIndex + fftSize, fftData.begin());
        
        //first apply a windowing function to our data
        window->multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(fftSize));
        
        //then render our fft data
        forwardFFT->performFrequencyOnlyForwardTransform(fftData.data());

        int numBins = (int)fftSize / 2;
        
        //normalise the fft values
        for( int i=0; i<numBins; ++i)
        {
            fftData[static_cast<std::vector<float>::size_type>(i)] /= (float) numBins;
        }
        
        //Conversion then to decibel
        for( int i = 0; i<numBins; ++i )
        {
            fftData[static_cast<std::vector<float>::size_type>(i)] = juce::Decibels::gainToDecibels(fftData[static_cast<std::vector<float>::size_type>(i)], negativeInfinity);
        }
        
        fftDataFifo.push(fftData);
    }
    
    void changeOrder(FFTOrder newOrder)
    {
        order = newOrder;
        auto fftSize = getFFTSize();
        
        forwardFFT = std::make_unique<juce::dsp::FFT>(order);
        window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize,
                                                                       juce::dsp::WindowingFunction<float>::blackmanHarris);
        
        fftData.clear();
        fftData.resize(static_cast<std::vector<float>::size_type>(fftSize * 2), 0);
        
        fftDataFifo.prepare(fftData.size());
    }
    //==============================================================================
    int getFFTSize() const {return 1 << order;}
    int getNumAvailableFFTDataBlocks() const {return fftDataFifo.getNumAvailableForReading();}
    //==============================================================================
    bool getFFTData(BlockType& result) {return fftDataFifo.pull(result);}
private:
    FFTOrder order;
    BlockType fftData;
    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    Fifo<BlockType> fftDataFifo;
};

template<typename PathType>
struct AnalyserPathGenerator
{
    /**
        converts 'renderData[ ]' into a juce::Path
     */
    void generatePath(const std::vector<float>& renderData,
                      juce::Rectangle<float> fftBounds,
                      int fftSize,
                      float binWidth,
                      float negativeInfinity)
    {
        auto top = fftBounds.getY();
        auto bottom = fftBounds.getHeight();
        auto width = fftBounds.getWidth();
        
        int numBins = (int)fftSize / 2;
        
        PathType p;
        p.preallocateSpace(3 * (int)fftBounds.getWidth());
        
        auto map = [bottom, top, negativeInfinity](float v)
        {
            return juce::jmap(v,
                              negativeInfinity, 0.f,
                              float(bottom), top);
        };
        
        auto y = map(renderData[0]);
        
        jassert( !std::isnan(y) && ! std::isinf(y) );
        
        p.startNewSubPath(0, y);
        
        const int pathResolution = 2; //you can draw line-to's every 'pathResolution' pixels.
        
        for (int binNum=1; binNum < numBins; binNum += pathResolution)
        {
            y = map(renderData[static_cast<std::vector<float>::size_type>(binNum)]);
            
            jassert( !std::isnan(y) && ! std::isinf(y) );
            
            if ( !std::isnan(y) && ! std::isinf(y) )
            {
                auto binFreq = binNum * binWidth;
                auto normalizedBinX = juce::mapFromLog10(binFreq, 20.f, 20000.f);
                int binX = static_cast<int>(std::floor(normalizedBinX * width));
                p.lineTo(binX, y);
            }
        }
        pathFifo.push(p);
    }
    
    int getNumPathsAvailable() const
    {
        return pathFifo.getNumAvailableForReading();
    }
    
    bool getPath(PathType& path)
    {
        return pathFifo.pull(path);
    }
    
private:
    Fifo<PathType> pathFifo;
};

struct CustomLookAndFeel : juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics&,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider&) override;
    
    void drawToggleButton (juce::Graphics &g,
                           juce::ToggleButton &toggleButton,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;
};

//Custom rotary slider
struct RotarySliderWithLabels : juce::Slider
{
    RotarySliderWithLabels(juce::RangedAudioParameter& rap, const juce::String& unitSuffix) :
    juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,juce::Slider::TextEntryBoxPosition::NoTextBox),
    param(&rap),
    suffix(unitSuffix)
    {
        setLookAndFeel(&lnf);
    }
    
    ~RotarySliderWithLabels() override
    {
        setLookAndFeel(nullptr);
    }
    
    struct LabelPos
    {
        float pos;
        juce::String label;
    };
    
    juce::Array<LabelPos> labels;
    void paint(juce::Graphics& g) override;
    juce::Rectangle<int> getSliderBounds() const;
    int getTextHeight() const {return 14;}
    juce::String getDisplayString() const;
    
private:
    CustomLookAndFeel lnf;
    juce::RangedAudioParameter* param;
    juce::String suffix;
};

struct PathProducer
{
    PathProducer(SingleChannelSampleFifo<ZooEQAudioProcessor::BlockType>& scsf) :
    leftChannelFifo(&scsf)
    {
        leftChannelFFTDataGenerator.changeOrder(FFTOrder::order2048);
        monoBuffer.setSize(1, leftChannelFFTDataGenerator.getFFTSize());
    }
    void process(juce::Rectangle<float> fftBounds, double sampleRate);
    juce::Path getPath() { return leftChannelFFTPath; }
    
private:
    SingleChannelSampleFifo<ZooEQAudioProcessor::BlockType>* leftChannelFifo;
    
    juce::AudioBuffer<float> monoBuffer;
    
    FFTDataGenerator<std::vector<float>> leftChannelFFTDataGenerator;
    
    AnalyserPathGenerator<juce::Path> pathProducer;

    juce::Path leftChannelFFTPath;
};

struct ResponseCurveComponent: juce::Component,
juce::AudioProcessorParameter::Listener,
juce::Timer
{
    ResponseCurveComponent(ZooEQAudioProcessor&);
    ~ResponseCurveComponent() override;
    
    void parameterValueChanged(int parameterIndex, float newValue) override;

    void parameterGestureChanged(int, bool) override {}
    
    void timerCallback() override;
    
    void paint(juce::Graphics& g) override;
    
    void resized() override;
    
    void toggleAnalysisEnablement(bool enabled)
    {
        shouldShowFFTAnalysis = enabled;
    }
    
private:
    ZooEQAudioProcessor& audioProcessor;
    juce::Atomic<bool> parametersChanged { false };
    
    MonoChain monoChain;
    
    void updateChain();
    
    juce::Image background;
    
    juce::Rectangle<int> getRenderArea();
    
    juce::Rectangle<int> getAnalysisArea();
    
    PathProducer leftPathProducer, rightPathProducer;
    
    bool shouldShowFFTAnalysis = true;
};

//==============================================================================

struct PowerButton : juce::ToggleButton { };
struct AnalyserButton : juce::ToggleButton
{
    void resized() override
    {
        auto bounds = getLocalBounds();
        auto insetRect = bounds.reduced(4);
        randomPath.clear();
        juce::Random r;
        
        randomPath.startNewSubPath(insetRect.getX(),
                                   insetRect.getY()+ insetRect.getHeight() * r.nextFloat());
        
        for ( auto x = insetRect.getX() + 1; x < insetRect.getRight(); x+= 2)
        {
            randomPath.lineTo(x, insetRect.getY()+ insetRect.getHeight() * r.nextFloat());
        }
    }
    juce::Path randomPath;
};

class ZooEQAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    ZooEQAudioProcessorEditor (ZooEQAudioProcessor&);
    ~ZooEQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    ZooEQAudioProcessor& audioProcessor;
    
    RotarySliderWithLabels peakFreqSlider,
    peakGainSlider,
    peakQualitySlider,
    lowCutFreqSlider,
    highCutFreqSlider,
    lowCutSlopeSlider,
    highCutSlopeSlider;
    
    ResponseCurveComponent responseCurveComponent;
    
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;
    Attachment peakFreqSliderAttachment,
                peakGainSliderAttachment,
                peakQualitySliderAttachment,
                lowCutFreqSliderAttachment,
                highCutFreqSliderAttachment,
                lowCutSlopeSliderAttachment,
                highCutSlopeSliderAttachment;
    
    PowerButton lowcutBypassButton, peakBypassButton, highcutBypassButton;
    AnalyserButton analyserEnableButton;
    
    
    using ButtonAttachment = APVTS::ButtonAttachment;
    ButtonAttachment    lowcutBypassButtonAttachment,
                        peakBypassButtonAttachment,
                        highcutBypassButtonAttachment,
                        analyserEnableButtonAttachment;
    
    std::vector<juce::Component*> getComps();
    
    CustomLookAndFeel lnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZooEQAudioProcessorEditor)
};
