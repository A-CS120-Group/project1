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
        auto openFile = new FileChooser("Choose file to send", File::getSpecialLocation(File::SpecialLocationType::userDesktopDirectory), "*.in");

        titleLabel.setText("Part3", juce::NotificationType::dontSendNotification);
        titleLabel.setSize(160, 40);
        titleLabel.setFont(juce::Font(36, juce::Font::FontStyleFlags::bold));
        titleLabel.setJustificationType(juce::Justification(juce::Justification::Flags::centred));
        titleLabel.setCentrePosition(300, 40);
        addAndMakeVisible(titleLabel);

        recordButton.setButtonText("Send");
        recordButton.setSize(80, 40);
        recordButton.setCentrePosition(150, 140);
        recordButton.onClick = [this, openFile] {
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
                inputBuffer.clear();
                status = 2;
            } else if (status == 2) {
                status = 3;
            }
            return;
        };
        addAndMakeVisible(playbackButton);

        setSize(600, 300);
        setAudioChannels(1, 1);
    }

    ~MainContentComponent() override { shutdownAudio(); }

private:
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
        // initialize carrier and preamble
        vector<double> t;
        t.reserve((int) sampleRate);
        std::cout << sampleRate << std::endl;
        for (int i = 0; i <= sampleRate; ++i) { t.push_back((double) i / sampleRate); }

        carrier.reserve((int) sampleRate);
        for (double i: t) { carrier.push_back(sin(2 * PI * 10000 * i)); }

        auto f = linspace(2000, 10000, 240);
        auto f_temp = linspace(10000, 2000, 240);
        f.reserve(f.size() + f_temp.size());
        f.insert(std::end(f), std::begin(f_temp), std::end(f_temp));

        std::vector<double> x(t.begin(), t.begin() + 480);
        preamble = cumtrapz(x, f);
        for (double &i: preamble) { i = sin(2 * PI * i); }
    }

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
                    const float *data = buffer->getReadPointer(channel);
                    for (int i = 0; i < bufferSize; ++i) { inputBuffer.push_back(*(data + i)); }
                    buffer->clear();
                } else if (status == 3) {
                    status = -1;
                    processInput();
                } else buffer->clear();
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
            case 3:
                titleLabel.setText("Processing", juce::NotificationType::dontSendNotification);
                break;
        }
    }

    void processInput() {
        double power = 0;
        int start_index = -1;
        std::deque<double> sync(480, 0);
        std::vector<double> decode;
        double syncPower_localMax = 0;
        int state = 0;// 0 sync; 1 decode
#ifdef WIN32
        juce::File writeTo(R"(C:\Users\caoster\Desktop\CS120\project1\)" + juce::Time::getCurrentTime().toISO8601(false) + ".out");
#else
        juce::File writeTo(juce::File::getCurrentWorkingDirectory().getFullPathName() + juce::Time::getCurrentTime().toISO8601(false) + ".out");
#endif

        for (int i = 0; i < inputBuffer.size(); ++i) {
            double cur = inputBuffer[i];
            power = power * (63.0 / 64) + cur * cur / 64;
            if (state == 0) {
                sync.pop_front();
                sync.push_back(cur);
                double syncPower = std::inner_product(sync.begin(), sync.end(), preamble.begin(), 0.0) / 200.0;
                if (syncPower > power * 2 && syncPower > syncPower_localMax && syncPower > 0.05) {
                    syncPower_localMax = syncPower;
                    start_index = i;
                } else if (i - start_index > 200 && start_index != -1) {
                    syncPower_localMax = 0;
                    sync = std::deque<double>(480, 0);
                    state = 1;
                    decode = std::vector<double>(inputBuffer.begin() + start_index + 1, inputBuffer.begin() + i + 1);
                    std::cout << "Header found" << std::endl;
                }
            } else {
                decode.push_back(cur);
                if (decode.size() == 48 * 108) {
                    std::transform(decode.begin(), decode.end(), carrier.begin(), decode.begin(), std::multiplies<>{});
                    decode = smooth(decode, 10);
                    std::vector<bool> bits(100);
                    for (int j = 0; j < 100; ++j) { bits[j] = 0 < std::accumulate(decode.begin() + 9 + j * 48, decode.begin() + 30 + j * 48, 0.0); }
                    int check = 0;
                    for (int j = 100; j < 108; ++j) check = (check << 1) | (0 < std::accumulate(decode.begin() + 9 + j * 48, decode.begin() + 30 + j * 48, 0.0));
                    if (check != crc8(bits)) {
                        std::cout << "Error" << i << "Total" << inputBuffer.size();
                    }
                    // Save in a file
                    for (int j = 0; j < 100; ++j) {
                        std::cout << bits[j];
                        writeTo.appendText(bits[j] ? "1" : "0");
                    }
                    std::cout << std::endl;
                    start_index = -1;
                    decode.clear();
                    state = 0;
                }
            }
        }
        std::cout << "Finish processing!" << std::endl;
        status = 0;
    }

    void releaseResources() override {}

    void generateSignal() {
        auto length = track.size();
        outputTrack.clear();

        size_t index = 0;
        for (int i = 0; i < floor(length / 100); ++i) {
            auto target = index + 100;
            vector<double> frame;
            vector<bool> crcFrame;
            frame.reserve(108);
            crcFrame.reserve(100);
            for (; index < target; ++index) {
                frame.push_back(track[index]);
                crcFrame.push_back(track[index]);
            }

            auto result = crc8(crcFrame);
            for (int j = 0; j < 8; ++j) {
                frame.push_back((result & 0x80) >> 7);
                result <<= 1;
            }
            // crc8 generated

            for (int j = 0; j < 50; ++j) { outputTrack.push_back(0); }
            outputTrack.insert(std::end(outputTrack), std::begin(preamble), std::end(preamble));

            for (int j = 0; j < frame.size(); ++j) {
                auto temp = frame[j] * 2 - 1;
                for (int k = 0; k < 48; ++k) { outputTrack.push_back(carrier[k + j * 48] * temp); }
            }
        }
        // Just in case
        for (int i = 0; i < 50; ++i) { outputTrack.push_back(0); }
        // The rest does not make 100 number
//        for (double &i: outputTrack) { inputBuffer.push_back(i); }
//        processInput();
    }

private:
    std::vector<bool> track;
    std::vector<double> outputTrack;
    std::vector<double> inputBuffer;

    juce::Label titleLabel;
    juce::TextButton recordButton;
    juce::TextButton playbackButton;

    int status{0};// 0 for waiting, 1 for sending, 2 for listening, 3 for processing, -1 for waiting
    long long startTime{0};
    int readPosition{0};

    vector<double> carrier;
    vector<double> preamble;// preamble sequence for synchronizing

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
