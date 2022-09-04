#include "utils.h"
#include <JuceHeader.h>
#include <chrono>
#pragma once

using juce::File;
using juce::FileChooser;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;


class MainContentComponent : public juce::AudioAppComponent {
public:
    MainContentComponent() {
        openFile = new FileChooser("Choose file to send", File::getSpecialLocation(File::SpecialLocationType::userDesktopDirectory), "*.in");

        titleLabel.setText("Part3", juce::NotificationType::dontSendNotification);
        titleLabel.setSize(160, 40);
        titleLabel.setFont(juce::Font(36, juce::Font::FontStyleFlags::bold));
        titleLabel.setJustificationType(juce::Justification(juce::Justification::Flags::centred));
        titleLabel.setCentrePosition(300, 40);
        addAndMakeVisible(titleLabel);

        recordButton.setButtonText("Send");
        recordButton.setSize(80, 40);
        recordButton.setCentrePosition(150, 140);
        recordButton.onClick = [this] {
            if (status != 0) { return; }
            openFile->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this](const FileChooser &chooser) {
                FileInputStream inputStream(chooser.getResult());
                if (inputStream.failedToOpen()) {
                    return;
                } else {
                    // Put all information in the track
                    juce::String inputString = inputStream.readString();
                    track.clear();
                    track.reserve(inputStream.getTotalLength());
                    for (auto _: inputString) {
                        auto nextChar = static_cast<char>(_);
                        if (nextChar == '0') {
                            track.push_back(false);
                        } else if (nextChar == '1') {
                            track.push_back(true);
                        }
                    }
                    status = 1;
                }
            });
        };
        addAndMakeVisible(recordButton);

        playbackButton.setButtonText("Receive");
        playbackButton.setSize(80, 40);
        playbackButton.setCentrePosition(450, 140);
        playbackButton.onClick = [this] {
            if (status == 1) {
                return;
            } else if (status == 2) {
                status = 0;
                return;
            }
        };
        addAndMakeVisible(playbackButton);

        setSize(600, 300);
        setAudioChannels(1, 1);
    }

    ~MainContentComponent() override { shutdownAudio(); }

private:
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
        this->_sampleRate = (int) sampleRate;
        this->delayBuffer = new juce::AudioSampleBuffer;
        this->delayBuffer->setSize(1, (int) this->getSampleRate() * 10);// 10 seconds in total
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
            if ((!activeInputChannels[channel] || !activeOutputChannels[channel]) || maxInputChannels == 0) {
                bufferToFill.buffer->clear(channel, bufferToFill.startSample, bufferToFill.numSamples);
            } else {
                auto channelData = buffer->getReadPointer(channel);
                if (status == 1) {
                    fillBuffer(channel, bufferSize, delayBufferSize, channelData);
                    buffer->clear();
                } else if (status == 3) {
                    readFromBuffer(channel, bufferSize, delayBufferSize, buffer);
                } else {
                    buffer->clear();
                }
            }
        }

        switch (status) {
            case 0:
                titleLabel.setText("Part3", juce::NotificationType::dontSendNotification);
                break;
            case 1:
                titleLabel.setText("Sending", juce::NotificationType::dontSendNotification);
                break;
            case 2:
                titleLabel.setText("Listening", juce::NotificationType::dontSendNotification);
                break;
        }
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
        writePosition += bufferSize;
        writePosition %= delayBufferSize;
    }

    void readFromBuffer(int channel, int bufferSize, int delayBufferSize, juce::AudioSampleBuffer *buffer) {
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
        readPosition += bufferSize;
        readPosition %= delayBufferSize;
    }

    void releaseResources() override {
        delete this->delayBuffer;
        delete this->openFile;
    }

    int getSampleRate() const { return _sampleRate; }

    void generateSignal() {
        vector<double> t;
        auto sampleRate = getSampleRate();
        t.reserve(sampleRate);
        for (int i = 0; i <= sampleRate; ++i) { t.push_back((double) i / sampleRate); }

        vector<double> carrier;
        carrier.reserve(sampleRate);
        for (double i: t) { carrier.push_back(2 * PI * i); }

        auto f = linspace(2000, 10000, 220);
        auto temp = linspace(10000, 2000, 220);
        f.reserve(f.size() + temp.size());
        f.insert(std::end(f), std::begin(temp), std::end(temp));

        std::vector<double> x(t.begin(), t.begin() + 440);
        auto preamble = cumtrapz(x, f);

        for (double &i: preamble) { i = sin(2 * PI * i); }

        auto length = track.size();
        size_t index = 0;
        for (int i = 0; i < floor(length / 100); ++i) {
            auto target = index + 100;
            vector<double> frame;
            frame.reserve(100);
            for (; index < target; ++index) {
                frame.push_back(track[index]);
            }
            // something about crc8 here
            auto result = crc8(track);
        }
        // The rest does not make 100 number
    }

private:
    FileChooser *openFile{nullptr};
    std::vector<bool> track;
    double header[440]{};

    juce::Label titleLabel;
    juce::TextButton recordButton;
    juce::TextButton playbackButton;

    int status{0};// 0 for waiting, 1 for sending, 2 for receiving
    long long startTime{0};
    int _sampleRate{0};
    juce::AudioSampleBuffer *delayBuffer{nullptr};
    int writePosition{0};
    int readPosition{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
