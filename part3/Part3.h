#include "utils.h"
#include <JuceHeader.h>
#include <chrono>
#include <fstream>

//#define Flash
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
        auto openFile = new FileChooser("Choose file to send",
                                        File::getSpecialLocation(File::SpecialLocationType::userDesktopDirectory),
                                        "*.in");

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
            openFile->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                  [this](const FileChooser &chooser) {
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
        std::ifstream file(getPath("part3/carrier.dat", 5));
        for (auto &x: carrier)
            file >> x.l;
        file.close();
        file.open(getPath("part3/preamble.dat", 5));
        for (auto &x: preamble)
            file >> x.l;
        file.close();
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
        Fixed power(0);
        int start_index = -1;
        std::deque<Fixed> sync(480, Fixed(0));
        std::vector<Fixed> decode;
        Fixed syncPower_localMax(0);
        int state = 0;// 0 sync; 1 decode
#ifdef Flash
        juce::File writeTo(R"(C:\Users\hujt\Desktop\)" + juce::Time::getCurrentTime().toISO8601(false) + ".out");
#else
#ifdef WIN32
        juce::File writeTo(R"(C:\Users\caoster\Desktop\CS120\project1\)" + juce::Time::getCurrentTime().toISO8601(false) + ".out");
#else
        juce::File writeTo(juce::File::getCurrentWorkingDirectory().getFullPathName() + juce::Time::getCurrentTime().toISO8601(false) + ".out");
#endif
#endif
        for (int i = 0; i < inputBuffer.size(); ++i) {
            Fixed cur(inputBuffer[i]);
            power = (power * Fixed(63) + cur * cur) / 64;
            if (state == 0) {
                sync.pop_front();
                sync.push_back(cur);
                Fixed syncPower(0);
                const Fixed *ptr = preamble;
                for (auto x: sync) {
                    syncPower = syncPower + *ptr * x;
                    ++ptr;
                }
                syncPower = syncPower / 200;
                if (syncPower > power * Fixed(2) && syncPower > syncPower_localMax && syncPower > Fixed(0.05)) {
                    syncPower_localMax = syncPower;
                    start_index = i;
                } else if (i - start_index > 200 && start_index != -1) {
                    syncPower_localMax = Fixed(0);
                    sync = std::deque<Fixed>(480, Fixed(0));
                    state = 1;
                    decode = std::vector<Fixed>(inputBuffer.begin() + start_index + 1, inputBuffer.begin() + i + 1);
                    std::cout << "Header found" << std::endl;
                }
            } else {
                decode.push_back(cur);
                if (decode.size() == 48 * 108) {
                    const Fixed *ptr = carrier;
                    for (auto &x: decode) {
                        x = x * *ptr;
                        ++ptr;
                    }
                    decode = smooth(decode, 10);
                    std::vector<bool> bits(100);
                    int check = 0;
                    for (int j = 0; j < 108; ++j) {
                        Fixed sum(0);
                        auto iter = decode.begin() + 9 + j * 48, iterEnd = decode.begin() + 30 + j * 48;
                        for (; iter != iterEnd; ++iter)
                            sum = sum + *iter;
                        bool curBit = sum > Fixed(0);
                        if (j < 100)
                            bits[j] = curBit;
                        else
                            check = (check << 1) | (int) curBit;
                    }
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
        for (int i = 0; i < length / 100; ++i) {
            auto target = index + 100;
            vector<bool> frame;
            vector<bool> crcFrame;
            frame.reserve(108);
            crcFrame.reserve(100);
            for (; index < target; ++index) {
                frame.push_back(track[index]);
                crcFrame.push_back(track[index]);
            }

            auto result = crc8(crcFrame);
            for (int j = 7; j >= 0; --j)
                frame.push_back((result >> j) & 1);
            // crc8 generated

            for (int j = 0; j < 50; ++j)
                outputTrack.push_back(0);
            for (auto x: preamble)
                outputTrack.push_back(x.to_double());

            for (int j = 0; j < frame.size(); ++j) {
                Fixed temp((int) frame[j] * 2 - 1);
                for (int k = 0; k < 48; ++k)
                    outputTrack.push_back((carrier[k + j * 48] * temp).to_double());
            }
        }
        // Just in case
        for (int i = 0; i < 50; ++i) { outputTrack.push_back(0); }
        // The rest does not make 100 number
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

    Fixed carrier[48001];
    Fixed preamble[480];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
