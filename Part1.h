#include <JuceHeader.h>
#include <chrono>
#pragma once

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;


//==============================================================================
class MainContentComponent : public juce::AudioAppComponent {
public:
    //==============================================================================
    MainContentComponent() {
        titleLabel.setText("Part1", juce::NotificationType::dontSendNotification);
        titleLabel.setSize(80, 40);
        titleLabel.setFont(juce::Font(36, juce::Font::FontStyleFlags::bold));
        titleLabel.setCentrePosition(300, 40);
        addAndMakeVisible(titleLabel);

        recordButton.setButtonText("Record");
        recordButton.setBounds(150, 120, 80, 40);
        recordButton.onClick = [this] {
            if (this->recordStart == 0) {
                this->recordStart = 1;
                this->startTime = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count();
            }
        };
        //        addAndMakeVisible(recordButton);

        setSize(600, 300);
        setAudioChannels(2, 2);
    }

    ~MainContentComponent() override { shutdownAudio(); }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
        this->_sampleRate = sampleRate;
        this->delayBuffer = new juce::AudioSampleBuffer;
        this->delayBuffer->setSize(2, (int) this->getSampleRate() * 10);// 10 seconds in total
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) override {
        auto *device = deviceManager.getCurrentAudioDevice();
        auto activeInputChannels = device->getActiveInputChannels();
        auto activeOutputChannels = device->getActiveOutputChannels();
        auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
        auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
        auto buffer = bufferToFill.buffer;
        auto delayBufferSize = delayBuffer->getNumSamples();
        auto bufferSize = buffer->getNumSamples();

        for (auto channel = 0; channel < maxOutputChannels; ++channel) {
            if ((!activeOutputChannels[channel]) || maxInputChannels == 0 || !activeInputChannels[channel]) {
                bufferToFill.buffer->clear(channel, bufferToFill.startSample, bufferToFill.numSamples);
            } else {
                if (recordStart == 1 && duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count() - this->startTime > 100000) { recordStart = 2; }

                auto channelData = buffer->getReadPointer(channel);
                //                    if (recordStart == 1) {
                fillBuffer(channel, bufferSize, delayBufferSize, channelData);
                //                    } else if (recordStart == 2) {
                readFromBuffer(channel, bufferSize, delayBufferSize, buffer);
                //                    }
            }
        }

        writePosition += bufferSize;
        writePosition %= delayBufferSize;
    }

    void fillBuffer(int channel, int bufferSize, int delayBufferSize, const float *channelData) {
        float gain = 1.0;
        if (delayBufferSize > bufferSize + writePosition) {
            delayBuffer->copyFromWithRamp(channel, writePosition, channelData, bufferSize, gain, gain);
        } else {
            auto numSamplesToEnd = delayBufferSize - writePosition;
            delayBuffer->copyFromWithRamp(channel, writePosition, channelData, numSamplesToEnd, gain, gain);
            auto numSamplesAtStart = bufferSize - numSamplesToEnd;
            delayBuffer->copyFromWithRamp(channel, 0, channelData + numSamplesToEnd, numSamplesAtStart, gain, gain);
        }
    }

    void readFromBuffer(int channel, int bufferSize, int delayBufferSize, juce::AudioSampleBuffer *buffer) {
        auto readPosition = writePosition - 2 * this->getSampleRate();
        if (readPosition < 0) { readPosition += delayBufferSize; }
        float gain = 1.0;
        buffer->clear();
        if (readPosition + bufferSize < delayBufferSize) {
            buffer->addFromWithRamp(channel, 0, delayBuffer->getReadPointer(channel, (int) readPosition), bufferSize, gain, gain);
        } else {
            auto numSamplesToEnd = delayBufferSize - readPosition;
            buffer->addFromWithRamp(channel, 0, delayBuffer->getReadPointer(channel, (int) readPosition), (int) numSamplesToEnd, gain, gain);
            auto numSamplesAtStart = bufferSize - numSamplesToEnd;
            buffer->addFromWithRamp(channel, (int) numSamplesToEnd, delayBuffer->getReadPointer(channel, 0), (int) numSamplesAtStart, gain, gain);
        }
    }

    void releaseResources() override {
        this->delayBuffer->setSize(0, 0);
        delete &this->delayBuffer;
    }

    void resized() override {
        //        titleLabel.setBounds(10, 10, 90, 20);
        //        levelSlider.setBounds(100, 10, getWidth() - 110, 20);
    }

    double getSampleRate() const { return _sampleRate; }

private:
    juce::Random random;
    juce::Label titleLabel;
    juce::TextButton recordButton;

    int recordStart{0};// 0 for not started, 1 for recording, 2 for finished recording, 3 for playing.
    long long startTime{0};
    double _sampleRate{0};
    juce::AudioSampleBuffer *delayBuffer{nullptr};
    int writePosition{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
