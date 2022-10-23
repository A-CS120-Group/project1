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

const int colTrack = 100;
const int rowTrack = 100;
const int colOutput = colTrack + CRCL;
const int rowOutput = rowTrack + CRCL;

class MainContentComponent : public juce::AudioAppComponent {
public:
    MainContentComponent() {
        openFile = new FileChooser("Choose file to send",
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
        recordButton.onClick = [this] {
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

    ~MainContentComponent() override {
        delete openFile;
        shutdownAudio();
    }

private:
    void prepareToPlay([[maybe_unused]]int samplesPerBlockExpected, [[maybe_unused]]double sampleRate) override {
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
                        buffer->addSample(channel, i, (float) outputTrack[readPosition].to_double());
                    }
                } else if (status == 2) {
                    // Receive sound here
                    const float *data = buffer->getReadPointer(channel);
                    for (int i = 0; i < bufferSize; ++i) { inputBuffer.emplace_back(Fixed((double) data[i])); }
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
#ifdef Flash
        inputBuffer = outputTrack;
#endif
        vector<int> rowError;
        vector<bool> output(rowOutput * colOutput);
        ECC ecc;
        int iFrame = 0;

        auto power = Fixed(0);
        int start_index = -1;
        std::deque<Fixed> sync(480, Fixed(0));
        std::vector<Fixed> decode;
        auto syncPower_localMax = Fixed(0);
        int state = 0;// 0 sync; 1 decode
        for (int iPos = 0; iPos < inputBuffer.size(); ++iPos) {
            Fixed cur(inputBuffer[iPos]);
            power = power * Fixed(63) / 64 + cur * cur / 64;
            if (state == 0) {
                sync.pop_front();
                sync.push_back(cur);
                auto syncPower = Fixed(0);
                const Fixed *ptr = preamble;
                for (auto x: sync) {
                    syncPower = syncPower + *ptr * x;
                    ++ptr;
                }
                syncPower = syncPower / 200;
                if (syncPower > power * Fixed(2) && syncPower > syncPower_localMax && syncPower > Fixed(1) / 20) {
                    syncPower_localMax = syncPower;
                    start_index = iPos;
                } else if (iPos - start_index > 200 && start_index != -1) {
                    syncPower_localMax = Fixed(0);
                    sync = std::deque<Fixed>(480, Fixed(0));
                    state = 1;
                    decode = std::vector<Fixed>(inputBuffer.begin() + start_index + 1, inputBuffer.begin() + iPos + 1);
                    //std::cerr << "Header found " << start_index << ' ' << iPos << std::endl;
                }
            } else {
                decode.push_back(cur);
                if (decode.size() == 48 * colOutput) {
                    const Fixed *ptr = carrier;
                    for (auto &x: decode) {
                        x = x * *ptr;
                        ++ptr;
                    }
                    decode = smooth(decode, 10);
                    std::vector<bool> frame(colOutput);
                    for (int j = 0; j < colOutput; ++j) {
                        auto sum = Fixed(0);
                        auto iter = decode.begin() + 8 + j * 48, iterEnd = decode.begin() + 40 + j * 48;
                        for (; iter != iterEnd; ++iter)
                            sum = sum + *iter;
                        frame[j] = sum > Fixed(0);
                    }
                    if (!crcCheck<CRCL>(frame)) {
                        std::cerr << "Row Error " << iFrame << " found\n";
                        ecc.searchUninformed(frame, 2);
                        if (ecc.res.empty())
                            rowError.push_back(iFrame);
                        else {
                            std::cerr << "Row Error " << iFrame << " corrected in the first step!\n";
                            assert(ecc.res.size() == 1);
                            frame = ecc.res[0].code;
                        }
                    }
                    for (int j = 0; j < colOutput; ++j)
                        output[iFrame * colOutput + j] = frame[j];
                    start_index = -1;
                    decode.clear();
                    state = 0;
                    ++iFrame;
                }
            }
        }
        vector<vector<bool>> colFrame(colTrack);
        vector<vector<bool>> colCorrected(colTrack);
        for (int j = 0; j < colTrack; ++j) {
            colFrame[j].resize(iFrame);
            colCorrected[j].resize(colOutput);
            for (int i = 0; i < iFrame; ++i)
                colFrame[j][i] = output[i * colOutput + j];
            if (!crcCheck<CRCL>(colFrame[j]))
                std::cerr << "Col Error " << j << " found\n";
        }
        int numMissing = rowOutput - iFrame;
        assert(numMissing >= 0);
        std::cerr << "numMissing = " << numMissing << '\n';
        vector<int> rowMissing(numMissing);
        for (int i = 0; i < numMissing; ++i)
            rowMissing[i] = i;
        while (true) {
            auto rowErrorAfter = rowError;
            for (auto i: rowMissing) {
                for (auto &x: rowErrorAfter)
                    if (i <= x) ++x;
            }
            bool solveAll = true;
            for (int j = 0; j < colTrack; ++j) {
                vector<bool> code = colFrame[j];
                for (auto i: rowMissing)
                    code.insert(code.cbegin() + i, false);
                ecc.search(code, (int) rowErrorAfter.size() + numMissing, rowErrorAfter, rowMissing);
                if (ecc.res.empty()) {
                    solveAll = false;
                    break;
                }
                colCorrected[j] = std::min_element(ecc.res.begin(), ecc.res.end(),
                                                   [](const ECCResult &x, const ECCResult &y) {
                                                       return x.hammingDistance < y.hammingDistance;
                                                   })->code;
            }
            if (solveAll) // find solution that satisfy all columns
                break;
            // get next rowMissing
            int p = numMissing - 1;
            while (p >= 0 && rowMissing[p] >= iFrame + p)
                --p;
            assert(p >= 0);
            ++rowMissing[p];
            for (++p; p < numMissing; ++p)
                rowMissing[p] = rowMissing[p - 1] + 1;
        }
        std::cout << "Finish signal decoding!" << std::endl;
        // Save in a file
#ifdef Flash
        #define FName R"(C:\Users\hujt\OneDrive - shanghaitech.edu.cn\G3 fall\Computer Network\Proj1\project1\output.out)"
        remove(FName);
        juce::File writeTo(FName);
#else
#ifdef WIN32
        juce::File writeTo(
                R"(C:\Users\caoster\Desktop\CS120\project1\)" + juce::Time::getCurrentTime().toISO8601(false) + ".out");
#else
        juce::File writeTo(juce::File::getCurrentWorkingDirectory().getFullPathName() + juce::Time::getCurrentTime().toISO8601(false) + ".out");
#endif
#endif
        for (int i = 0; i < rowTrack; ++i) {
            for (int j = 0; j < colTrack; ++j)
                writeTo.appendText(colCorrected[j][i] ? "1" : "0");
        }
        status = 0;
    }

    void releaseResources() override {}

    void generateSignal() {
        outputTrack.clear();
        vector<unsigned int> colCRC(colTrack);
        for (int j = 0; j < colTrack; ++j) {
            vector<bool> frame(rowTrack);
            for (int i = 0; i < rowTrack; ++i)
                frame[i] = track[i * colTrack + j];
            colCRC[j] = crc<CRCL>(frame);
        }

        for (int i = 0; i < rowOutput; ++i) {
            vector<bool> frame;
            frame.reserve(colOutput);
            if (i < rowTrack) {
                for (int j = 0; j < colTrack; ++j)
                    frame.push_back(track[i * colTrack + j]);
            } else {
                for (int j = 0; j < colTrack; ++j)
                    frame.push_back((colCRC[j] >> (CRCL - 1 - (i - rowTrack))) & 1);
            }
            auto rowCRC = crc<CRCL>(frame);
            for (int j = 0; j < CRCL; ++j)
                frame.push_back((rowCRC >> (CRCL - 1 - j)) & 1);
            // crc generated
            for (int j = 0; j < 50; ++j)
                outputTrack.emplace_back(Fixed(0));
            for (auto x: preamble)
                outputTrack.push_back(x);
            for (int j = 0; j < colOutput; ++j) {
                if (frame[j]) {
                    for (int k = 0; k < 48; ++k)
                        outputTrack.emplace_back(carrier[k + j * 48]);
                } else {
                    for (int k = 0; k < 48; ++k)
                        outputTrack.emplace_back(-carrier[k + j * 48]);
                }
            }
        }
        // Just in case
        for (int i = 0; i < 50; ++i)
            outputTrack.emplace_back(Fixed(0));
    }

private:
    FileChooser *openFile;

    std::vector<bool> track;
    std::vector<Fixed> outputTrack;
    std::vector<Fixed> inputBuffer;

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
