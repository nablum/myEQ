/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ZooEQAudioProcessor::ZooEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

ZooEQAudioProcessor::~ZooEQAudioProcessor()
{
}

//==============================================================================
const juce::String ZooEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ZooEQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ZooEQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ZooEQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ZooEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ZooEQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ZooEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ZooEQAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String ZooEQAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void ZooEQAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void ZooEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    
    juce::dsp::ProcessSpec spec;
    
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    
    spec.numChannels=1;
    
    spec.sampleRate=sampleRate;
    
    leftChain.prepare(spec);
    rightChain.prepare(spec);
    
    // === Filter Processing === //
    updateFilters();
    
    // === Fifo process === //
    leftChannelFifo.prepare(samplesPerBlock);
    rightChannelFifo.prepare(samplesPerBlock);
    
    osc.initialise([](float x) { return std::sin(x); } );
    
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());
    osc.prepare(spec);
    osc.setFrequency(5000);
    
}

void ZooEQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ZooEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void ZooEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // === Filter Processing === //
    updateFilters();
    
    // === Apply FX on the audio === //
    juce::dsp::AudioBlock<float> block(buffer);
    
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    
    leftChain.process(leftContext);
    rightChain.process(rightContext);
    
    leftChannelFifo.update(buffer);
    rightChannelFifo.update(buffer);
}

//==============================================================================
bool ZooEQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ZooEQAudioProcessor::createEditor()
{
    return new ZooEQAudioProcessorEditor (*this);
//    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void ZooEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    
    //This command is to save your parameters when you quit the plugin
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
    
}

void ZooEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    
    //This command is to save your parameters when you quit the plugin
    auto tree = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if ( tree.isValid() )
    {
        apvts.replaceState(tree);
        updateFilters();
    }
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;
    
    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());
    settings.lowCutBypassed = apvts.getRawParameterValue("LowCut Bypassed")->load() > 0.5f;
    settings.peakBypassed = apvts.getRawParameterValue("Peak Bypassed")->load() > 0.5f;
    settings.highCutBypassed = apvts.getRawParameterValue("HighCut Bypassed")->load() > 0.5f;
    
    return settings;
}

Coefficients makePeakFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                               chainSettings.peakFreq,
                                                               chainSettings.peakQuality,
                                                               juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
}

void ZooEQAudioProcessor::updatePeakFilter(const ChainSettings& chainSettings)
{
    //Read if filter is bypassed
    leftChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    rightChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    
    auto peakCoefficients = makePeakFilter(chainSettings, getSampleRate());
    updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
}

void updateCoefficients(Coefficients &old, const Coefficients &replacements)
{
    *old = *replacements;
}

void ZooEQAudioProcessor::updateLowCutFilters(const ChainSettings &chainSettings)
{
    //Read if filter is bypassed
    leftChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    rightChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    
    //Definition of the low cut filter coefficients
    auto lowCutCoefficients = makeLowCutFilter(chainSettings, getSampleRate());
    
    //Set the LowCut Filter Coeffecients to the left chain
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    updateCutFilter(leftLowCut, lowCutCoefficients, chainSettings.lowCutSlope);
    
    //Set the LowCut Filter Coeffecients to the right chain
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    updateCutFilter(rightLowCut, lowCutCoefficients, chainSettings.lowCutSlope);
}

void ZooEQAudioProcessor::updateHighCutFilter(const ChainSettings &chainSettings)
{
    //Read if filter is bypassed
    leftChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    rightChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    
    //Definition of the high cut filter coefficients
    auto highCutCoefficients = makeHighCutFilter(chainSettings, getSampleRate());
    
    //Set the HighCut Filter Coeffecients to the left chain
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    updateCutFilter(leftHighCut, highCutCoefficients, chainSettings.highCutSlope);
    
    //Set the HighCut Filter Coeffecients to the right chain
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();
    updateCutFilter(rightHighCut, highCutCoefficients, chainSettings.highCutSlope);
}

void ZooEQAudioProcessor::updateFilters()
{
    auto chainSettigns = getChainSettings(apvts);
    updateLowCutFilters(chainSettigns);
    updatePeakFilter(chainSettigns);
    updateHighCutFilter(chainSettigns);
}

juce::AudioProcessorValueTreeState::ParameterLayout
    ZooEQAudioProcessor::createParameterLayout() //Parameters of the plugin (Cut/Peak/Gain/Quality/Slope)
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq",
                                                           "LowCut Freq",
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
                                                           20.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut Freq",
                                                           "HighCut Freq",
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
                                                           20000.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Freq",
                                                           "Peak Freq",
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
                                                           750.f));
        
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain",
                                                           "Peak Gain",
                                                           juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f),
                                                           0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Quality",
                                                           "Peak Quality",
                                                           juce::NormalisableRange<float>(0.1f, 10.f, 0.5f, 1.f),
                                                           1.f));

    juce::StringArray stringArray;
        for (int i = 0; i<4; ++i) {
            juce::String str;
            str << (12 + i*12);
            str << "dB/Oct";
            stringArray.add(str);
        }

        layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCut Slope", stringArray, 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCut Slope", stringArray, 0));
    
    layout.add(std::make_unique<juce::AudioParameterBool>("LowCut Bypassed", "LowCut Bypassed", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("Peak Bypassed", "Peak Bypassed", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("HighCut Bypassed", "HighCut Bypassed", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("Analyser Enable", "Analyser Enable", true));
 
    return layout;
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZooEQAudioProcessor();
}
