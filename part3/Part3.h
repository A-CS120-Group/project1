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
                    generateSignal();
                    writePosition = 0;
                    readPosition = 0;
                    status = 1;
                }
            });
        };
        addAndMakeVisible(recordButton);

        playbackButton.setButtonText("Receive");
        playbackButton.setSize(80, 40);
        playbackButton.setCentrePosition(450, 140);
        playbackButton.onClick = [this] {
            if (status == 0) {
                status = 2;
            } else if (status == 2) {
                status = 0;
            }
            return;
        };
        addAndMakeVisible(playbackButton);

        setSize(600, 300);
        setAudioChannels(1, 1);
    }

    ~MainContentComponent() override { shutdownAudio(); }

private:
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override { this->_sampleRate = (int) sampleRate; }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) override {
        auto *device = deviceManager.getCurrentAudioDevice();
        auto activeInputChannels = device->getActiveInputChannels();
        auto activeOutputChannels = device->getActiveOutputChannels();
        auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
        auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
        auto buffer = bufferToFill.buffer;
        auto bufferSize = buffer->getNumSamples();

        for (auto channel = 0; channel < maxOutputChannels; ++channel) {
            if ((!activeInputChannels[channel] || !activeOutputChannels[channel]) || maxInputChannels == 0) {
                bufferToFill.buffer->clear(channel, bufferToFill.startSample, bufferToFill.numSamples);
            } else {
                if (status == 1) {
                    buffer->clear();
                    for (int i = 0; i < bufferSize; ++i, ++readPosition) {
                        if (readPosition >= outputTrack.size()) {
                            status = 0;
                            break;
                        }
                        buffer->addSample(channel, i, (float) outputTrack[readPosition]);
                    }
                } else if (status == 2) {
                    // Receive sound here

                    buffer->clear();
                }
            }
        }

        const MessageManagerLock mmLock;
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

    void releaseResources() override { delete this->openFile; }

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
        auto f_temp = linspace(10000, 2000, 220);
        f.reserve(f.size() + f_temp.size());
        f.insert(std::end(f), std::begin(f_temp), std::end(f_temp));

        std::vector<double> x(t.begin(), t.begin() + 440);
        auto preamble = cumtrapz(x, f);

        for (double &i: preamble) { i = sin(2 * PI * i); }

        auto length = track.size();
        outputTrack.clear();

        size_t index = 0;
        for (int i = 0; i < floor(length / 100); ++i) {
            auto target = index + 100;
            vector<double> frame;
            frame.reserve(108);
            for (; index < target; ++index) { frame.push_back(track[index]); }

            auto result = crc8(track);
            for (int j = 0; j < 8; ++j) {
                frame.push_back((result & 0x80) >> 7 == 1);
                result <<= 1;
            }
            // crc8 generated

            for (int j = 0; j < 50; ++j) { outputTrack.push_back(0); }
            outputTrack.insert(std::end(outputTrack), std::begin(preamble), std::end(preamble));

            for (int j = 0; j < frame.size(); ++j) {
                auto temp = frame[j] * 2 - 1;
                for (int k = 0; k < 44; ++k) { outputTrack.push_back(carrier[k + j * 44] * temp); }
            }
        }
        // Just in case
        for (int i = 0; i < 50; ++i) { outputTrack.push_back(0); }
        // The rest does not make 100 number
    }

private:
    FileChooser *openFile{nullptr};
    std::vector<bool> track;
    std::vector<double> outputTrack;
    double header[440]{};

    juce::Label titleLabel;
    juce::TextButton recordButton;
    juce::TextButton playbackButton;

    int status{0};// 0 for waiting, 1 for sending, 2 for receiving
    long long startTime{0};
    int _sampleRate{0};
    int readPosition{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
