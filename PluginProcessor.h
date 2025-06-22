#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

class Wavetable
{
public:
    Wavetable()
    {
        juce::AudioFormatManager manager;
        juce::FileChooser chooser("Please select the wavetable...", juce::File(), manager.getWildcardForAllFormats());
        juce::File file(chooser.getResult());
        juce::AudioFormatReader *reader = manager.createReaderFor(file);

        auto numChannels = reader->numChannels;
        auto lengthInSamples = reader->lengthInSamples;

        table.setSize((int)numChannels, (int)lengthInSamples);
        
        reader->read(&table,                     
                    0,                       
                    lengthInSamples, 
                    0,                           
                    true,                        
                    true);

        const int tableSize = 2048;
        // table.setSize(1, tableSize);
        // auto *samples = table.getWritePointer(0);
        // for (int i = 0; i < tableSize; ++i)
        // {
        //     float phase = (float)i / (float)tableSize * juce::MathConstants<float>::twoPi;
        //     samples[i] = std::sin(phase);
        // }

        tableLength = tableSize;
        tableN = lengthInSamples / tableSize;
    }

    float getSample(float phase)
    {
        phase = juce::jlimit(0.0f, 1.0f, phase);
        auto index = static_cast<int>(phase * tableLength) % tableLength;
        return table.getSample(0, index);
    }

    int getLength() const { return tableLength; }

private:
    juce::AudioBuffer<float> table;
    int tableLength;
    int tableN;
};

struct WavetableSound : public juce::SynthesiserSound
{
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

struct WavetableVoice : public juce::SynthesiserVoice
{
    WavetableVoice(Wavetable &wt) : wavetable(wt) {}

    bool canPlaySound(juce::SynthesiserSound *sound) override
    {
        return dynamic_cast<WavetableSound *>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound *, int /*currentPitchWheelPosition*/) override
    {
        phase = 0.0;
        level = velocity * 0.15;
        tailOff = 0.0;

        frequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        increment = frequency / getSampleRate();
    }

    void stopNote(float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            if (tailOff == 0.0)
                tailOff = 1.0;
        }
        else
        {
            clearCurrentNote();
            phase = 0.0;
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioSampleBuffer &outputBuffer, int startSample, int numSamples) override
    {
        if (!isVoiceActive())
            return;

        if (tailOff > 0.0)
        {
            while (--numSamples >= 0)
            {
                auto currentSample = wavetable.getSample(phase) * level * tailOff;

                for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                    outputBuffer.addSample(i, startSample, currentSample);

                ++startSample; // ← これが必要！

                phase += increment;
                if (phase >= 1.0f)
                    phase -= 1.0f;

                tailOff *= 0.99;

                if (tailOff <= 0.005)
                {
                    clearCurrentNote();
                    increment = 0.0;
                    break;
                }
            }
        }
        else
        {
            while (--numSamples >= 0)
            {
                auto currentSample = wavetable.getSample(phase) * level;

                for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                    outputBuffer.addSample(i, startSample, currentSample);

                ++startSample; // ← これも必要！

                phase += increment;
                if (phase >= 1.0f)
                    phase -= 1.0f;
            }
        }
    }

private:
    Wavetable &wavetable;
    double phase = 0.0, increment = 0.0, frequency = 440.0f, level = 0.0, tailOff = 0.0;
};

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

private:
    Wavetable wt;
    juce::MidiKeyboardState keyboardState;
    juce::Synthesiser synth;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};