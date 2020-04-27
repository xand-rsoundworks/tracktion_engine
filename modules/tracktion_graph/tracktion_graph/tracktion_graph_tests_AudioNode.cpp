/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


namespace tracktion_graph
{

using namespace test_utilities;

//==============================================================================
//==============================================================================
class AudioNodeTests : public juce::UnitTest
{
public:
    AudioNodeTests()
        : juce::UnitTest ("AudioNode", "AudioNode")
    {
    }
    
    void runTest() override
    {
        for (double sampleRate : { 44100.0, 48000.0, 96000.0 })
        {
            for (int blockSize : { 64, 256, 512, 1024 })
            {
                for (bool randomiseBlockSizes : { false, true })
                {
                    TestSetup setup { sampleRate, blockSize, randomiseBlockSizes, getRandom() };
                    logMessage (juce::String ("Test setup: sample rate SR, block size BS, random blocks RND")
                                .replace ("SR", juce::String (sampleRate))
                                .replace ("BS", juce::String (blockSize))
                                .replace ("RND", randomiseBlockSizes ? "Y" : "N"));

                    // Mono tests
                    runSinTests (setup);
                    runSinCancellingTests (setup);
                    runSinOctaveTests (setup);
                    runSendReturnTests (setup);
                    runLatencyTests (setup);

                    // MIDI tests
                    runMidiTests (setup);

                    // Multi channel tests
                    runStereoTests (setup);
                }
            }
        }
    }

private:
    //==============================================================================
    //==============================================================================
    void runSinTests (TestSetup testSetup)
    {
        beginTest ("Sin");
        {
            auto sinNode = std::make_unique<SinAudioNode> (220.0f);
            
            auto testContext = createBasicTestContext (std::move (sinNode), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
        }
    }

    void runSinCancellingTests (TestSetup testSetup)
    {
        beginTest ("Sin cancelling");
        {
            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (220.0f));

            auto sinNode = std::make_unique<SinAudioNode> (220.0f);
            auto invertedSinNode = std::make_unique<FunctionAudioNode> (std::move (sinNode), [] (float s) { return -s; });
            nodes.push_back (std::move (invertedSinNode));

            auto sumNode = std::make_unique<BasicSummingAudioNode> (std::move (nodes));
            
            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 0.0f, 0.0f);
        }
    }

    void runSinOctaveTests (TestSetup testSetup)
    {
        beginTest ("Sin octave");
        {
            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (220.0f));
            nodes.push_back (std::make_unique<SinAudioNode> (440.0f));

            auto sumNode = std::make_unique<BasicSummingAudioNode> (std::move (nodes));
            auto node = std::make_unique<FunctionAudioNode> (std::move (sumNode), [] (float s) { return s * 0.5f; });
            
            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 0.885f, 0.5f);
        }
    }
    
    void runSendReturnTests (TestSetup testSetup)
    {
        beginTest ("Sin send/return");
        {
            // Track 1 sends a sin tone to a send and then gets muted
            auto sinLowerNode = std::make_unique<SinAudioNode> (220.0f);
            auto sendNode = std::make_unique<SendAudioNode> (std::move (sinLowerNode), 1);
            auto track1Node = std::make_unique<FunctionAudioNode> (std::move (sendNode), [] (float) { return 0.0f; });

            // Track 2 has a silent source and receives input from the send
            auto sinUpperNode = std::make_unique<SinAudioNode> (440.0f);
            auto silentNode = std::make_unique<FunctionAudioNode> (std::move (sinUpperNode), [] (float) { return 0.0f; });
            auto track2Node = std::make_unique<ReturnAudioNode> (std::move (silentNode), 1);

            // Track 1 & 2 then get summed together
            auto node = makeBaicSummingAudioNode ({ track1Node.release(), track2Node.release() });

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
        }

        beginTest ("Sin send/return different bus#");
        {
            // This test is the same as before but uses a different bus number for the return so the output should be silent

            // Track 1 sends a sin tone to a send and then gets muted
            auto sinLowerNode = std::make_unique<SinAudioNode> (220.0f);
            auto sendNode = std::make_unique<SendAudioNode> (std::move (sinLowerNode), 1);
            auto track1Node = std::make_unique<FunctionAudioNode> (std::move (sendNode), [] (float) { return 0.0f; });

            // Track 2 has a silent source and receives input from the send
            auto sinUpperNode = std::make_unique<SinAudioNode> (440.0f);
            auto silentNode = std::make_unique<FunctionAudioNode> (std::move (sinUpperNode), [] (float) { return 0.0f; });
            auto track2Node = std::make_unique<ReturnAudioNode> (std::move (silentNode), 2);

            // Track 1 & 2 then get summed together
            auto node = makeBaicSummingAudioNode ({ track1Node.release(), track2Node.release() });

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 0.0f, 0.0f);
        }
        
        beginTest ("Sin send/return non-blocking");
        {
            // Track 1 sends a sin tone to a send with a gain of 0.25
            auto sinLowerNode = std::make_unique<SinAudioNode> (220.0f);
            auto attenuatedSinLowerNode = std::make_unique<FunctionAudioNode> (std::move (sinLowerNode), [] (float s) { return s * 0.25f; });
            auto track1Node = std::make_unique<SendAudioNode> (std::move (attenuatedSinLowerNode), 1);

            // Track 2 has a sin source  of gain 0.5 and receives input from the send
            auto sinUpperNode = std::make_unique<SinAudioNode> (440.0f);
            auto attenuatedSinUpperNode = std::make_unique<FunctionAudioNode> (std::move (sinUpperNode), [] (float s) { return s * 0.5f; });
            auto track2Node = std::make_unique<ReturnAudioNode> (std::move (attenuatedSinUpperNode), 1);

            // Track 1 & 2 then get summed together
            auto node = makeBaicSummingAudioNode ({ track1Node.release(), track2Node.release() });
            
            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 0.885f, 0.5f);
        }
    }
    
    void runLatencyTests (TestSetup testSetup)
    {
        beginTest ("Basic latency test cancelling sin");
        {
            /*  Has two sin nodes at the same frequency and delays one of them by half a period
                so they should cancel out completely.
                This uses the test sample rate to determine the sin frequency to avoid rounding errors.
            */
            const double sampleRate = testSetup.sampleRate;
            const double sinFrequency = sampleRate / 100.0;
            const double numSamplesPerCycle = sampleRate / sinFrequency;
            const int numLatencySamples = juce::roundToInt (numSamplesPerCycle / 2.0);

            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> ((float) sinFrequency));

            auto sinNode = std::make_unique<SinAudioNode> ((float) sinFrequency);
            auto latencySinNode = makeAudioNode<LatencyAudioNode> (std::move (sinNode), numLatencySamples);
            nodes.push_back (std::move (latencySinNode));

            auto sumNode = std::make_unique<BasicSummingAudioNode> (std::move (nodes));

            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, 5.0);

            // Start of buffer is +-1, after latency comp kicks in, the second half will be silent
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 1.0f, 0.707f, 0.0f, 0.0f);
        }
        
        beginTest ("Basic latency test doubling sin");
        {
            /*  This is the same test as before, two sin waves with one delayed but now the second one is compensated for.
                This has two implications:
                 1. There will be a half period of silence at the start of the output
                 2. Instead of cancelling, the sins will now constructively interfere, doubling the magnitude
            */
            const double sampleRate = testSetup.sampleRate;
            const double sinFrequency = sampleRate / 100.0;
            const double numSamplesPerCycle = sampleRate / sinFrequency;
            const int numLatencySamples = juce::roundToInt (numSamplesPerCycle / 2.0);

            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (makeGainNode (makeAudioNode<SinAudioNode> ((float) sinFrequency), 0.5f));

            auto sinNode = makeGainNode (makeAudioNode<SinAudioNode> ((float) sinFrequency), 0.5f);
            auto latencySinNode = makeAudioNode<LatencyAudioNode> (std::move (sinNode), numLatencySamples);
            nodes.push_back (std::move (latencySinNode));

            auto sumNode = makeAudioNode<SummingAudioNode> (std::move (nodes));

            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, 5.0);
            // Start of buffer which should be silent
            // Part of buffer after latency which should be all sin +-1.0
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
        }

        beginTest ("Send/return with latency");
        {
            /*  This has a sin input to a latency node leading to a send.
                The return branch also has a sin input. The latency should be compensated on the return node correctly.
            */
            const double sinFrequency = testSetup.sampleRate / 100.0;
            const double numSamplesPerCycle = testSetup.sampleRate / sinFrequency;
            const int numLatencySamples = juce::roundToInt (numSamplesPerCycle / 2.0);

            auto track1 = makeAudioNode<SinAudioNode> ((float) sinFrequency);
            track1 = makeAudioNode<LatencyAudioNode> (std::move (track1), numLatencySamples);
            track1 = makeGainNode (std::move (track1), 0.5f);
            track1 = makeAudioNode<SendAudioNode> (std::move (track1), 1);
            track1 = makeGainNode (std::move (track1), 0.0f);

            auto track2 = makeAudioNode<SinAudioNode> ((float) sinFrequency);
            track2 = makeGainNode (std::move (track2), 0.5f);
            track2 = makeAudioNode<ReturnAudioNode> (std::move (track2), 1);
            
            auto node = makeSummingAudioNode ({ track1.release(), track2.release() });

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);

            // Start of buffer which should be silent
            // Part of buffer after latency which should be all sin +-1.0
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
        }

        beginTest ("Multiple send/return with latency");
        {
            /*  This two tracks with sin input to a latency node leading to a send. The latency is different on each branch.
                The latency should be compensated on the return node correctly.
            */
            const double sinFrequency = testSetup.sampleRate / 100.0;
            const double numSamplesPerCycle = testSetup.sampleRate / sinFrequency;
            const int numLatencySamples = juce::roundToInt (numSamplesPerCycle / 2.0);

            auto track1 = makeAudioNode<SinAudioNode> ((float) sinFrequency);
            track1 = makeAudioNode<LatencyAudioNode> (std::move (track1), numLatencySamples);
            track1 = makeGainNode (std::move (track1), 0.5f);
            track1 = makeAudioNode<SendAudioNode> (std::move (track1), 1);
            track1 = makeGainNode (std::move (track1), 0.0f);

            auto track2 = makeAudioNode<SinAudioNode> ((float) sinFrequency);
            track2 = makeAudioNode<LatencyAudioNode> (std::move (track2), numLatencySamples * 2);
            track2 = makeGainNode (std::move (track2), 0.5f);
            track2 = makeAudioNode<SendAudioNode> (std::move (track2), 1);
            track2 = makeGainNode (std::move (track2), 0.0f);

            auto track3 = makeAudioNode<SinAudioNode> ((float) sinFrequency);
            track3 = makeGainNode (std::move (track3), 0.0f);
            track3 = makeAudioNode<ReturnAudioNode> (std::move (track3), 1);
            
            auto node = makeSummingAudioNode ({ track1.release(), track2.release(), track3.release() });

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);

            // Start of buffer which should be silent
            // Part of buffer after latency which should be all sin +-1.0
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
        }
        
        beginTest ("Send, send/return with two stage latency");
        {
            /*  This has a sin input to a latency node leading to a another latency block and another send on a different bus.
                There are then two other tracks that receive each of the send nodes.
                The latency should be compensated for and the output a mag 1 sin.
            */
            const double sinFrequency = testSetup.sampleRate / 100.0;
            const double numSamplesPerCycle = testSetup.sampleRate / sinFrequency;
            const int numLatencySamples = juce::roundToInt (numSamplesPerCycle / 2.0);

            auto track1 = makeAudioNode<SinAudioNode> ((float) sinFrequency);
            track1 = makeAudioNode<LatencyAudioNode> (std::move (track1), numLatencySamples);
            track1 = makeGainNode (std::move (track1), 0.5f);
            track1 = makeAudioNode<SendAudioNode> (std::move (track1), 1);
            track1 = makeAudioNode<LatencyAudioNode> (std::move (track1), numLatencySamples);
            track1 = makeAudioNode<SendAudioNode> (std::move (track1), 2);
            track1 = makeGainNode (std::move (track1), 0.0f);

            auto track2 = makeAudioNode<SilentAudioNode> (1);
            track2 = makeAudioNode<ReturnAudioNode> (std::move (track2), 1);

            auto track3 = makeAudioNode<SilentAudioNode> (1);
            track3 = makeAudioNode<ReturnAudioNode> (std::move (track3), 2);

            auto node = makeSummingAudioNode ({ track1.release(), track2.release(), track3.release() });

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);

            // Start of buffer which should be silent
            // Part of buffer after latency which should be all sin +-1.0
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
        }
    }
        
    void runMidiTests (TestSetup testSetup)
    {
        const double sampleRate = 44100.0;
        const double duration = 5.0;
        const auto sequence = test_utilities::createRandomMidiMessageSequence (duration, testSetup.random);
        
        beginTest ("Basic MIDI");
        {
            auto node = std::make_unique<MidiAudioNode> (sequence);
            
            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, duration);

            expectGreaterThan (sequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, sequence);
        }
        
        beginTest ("Delayed MIDI");
        {
            expectGreaterThan (sequence.getNumEvents(), 0);
            
            const int latencyNumSamples = juce::roundToInt (sampleRate / 100.0);
            const double delayedTime = latencyNumSamples / sampleRate;
            auto midiNode = std::make_unique<MidiAudioNode> (sequence);
            auto delayedNode = makeAudioNode<LatencyAudioNode> (std::move (midiNode), latencyNumSamples);

            auto testContext = createBasicTestContext (std::move (delayedNode), testSetup, 1, duration);
            
            auto extectedSequence = sequence;
            extectedSequence.addTimeToMessages (delayedTime);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, extectedSequence);
        }

        beginTest ("Compensated MIDI");
        {
            // This test has a sin node being delayed by a block which then gets mixed with a non-deleayed MIDI stream
            // The MIDI stream should be delayed by the same amount as the sin stream
            expectGreaterThan (sequence.getNumEvents(), 0);
            
            const int latencyNumSamples = juce::roundToInt (sampleRate / 100.0);
            const double delayedTime = latencyNumSamples / sampleRate;

            auto sinNode = makeAudioNode<SinAudioNode> (220.0f);
            auto delayedNode = makeAudioNode<LatencyAudioNode> (std::move (sinNode), latencyNumSamples);
            
            auto midiNode = makeAudioNode<MidiAudioNode> (sequence);
            auto summedNode = makeSummingAudioNode ({ delayedNode.release(), midiNode.release() });

            auto testContext = createBasicTestContext (std::move (summedNode), testSetup, 1, duration);
            
            auto extectedSequence = sequence;
            extectedSequence.addTimeToMessages (delayedTime);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, extectedSequence);
        }

        beginTest ("Send/return MIDI");
        {
            // Test that sends MIDI from one branch of a node to another and mutes the original
            const int busNum = 1;
            
            auto track1 = makeAudioNode<MidiAudioNode> (sequence);
            track1 = makeAudioNode<SendAudioNode> (std::move (track1), busNum);
            track1 = makeAudioNode<FunctionAudioNode> (std::move (track1), [] (float) { return 0.0f; });

            auto track2 = makeAudioNode<ReturnAudioNode> (makeAudioNode<SinAudioNode> (220.0f), busNum);
            
            auto sumNode = makeSummingAudioNode ({ track1.release(), track2.release() });

            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, duration);

            expectGreaterThan (sequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, sequence);
        }
        
        beginTest ("Send/return MIDI passthrough");
        {
            // Test that sends MIDI from one branch of a node to another and mutes the return path
            const int busNum = 1;
            
            auto track1 = makeAudioNode<MidiAudioNode> (sequence);
            track1 = makeAudioNode<SendAudioNode> (std::move (track1), busNum);

            auto track2 = makeAudioNode<ReturnAudioNode> (makeAudioNode<SinAudioNode> (220.0f), busNum);
            track2 = makeAudioNode<FunctionAudioNode> (std::move (track2), [] (float) { return 0.0f; });

            auto sumNode = makeSummingAudioNode ({ track1.release(), track2.release() });

            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, duration);

            expectGreaterThan (sequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, sequence);
        }
    }
    
    void runStereoTests (TestSetup testSetup)
    {
        beginTest ("Stereo sin");
        {
            auto node = makeAudioNode<SinAudioNode> (220.0f, 2);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 2, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 1.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.707f, 0.001f);
        }

        beginTest ("Stereo sin");
        {
            // Two mono sin nodes summed to L/R stereo
            auto leftSin = makeAudioNode<SinAudioNode> (220.0f, 1);
            auto rightSin = makeAudioNode<SinAudioNode> (220.0f, 1);

            std::vector<std::pair<int, int>> channelMap;
            channelMap.push_back ({ 0, 1 });
            auto rightRemapped = makeAudioNode<ChannelMappingAudioNode> (std::move (rightSin), channelMap, true);

            auto node = makeSummingAudioNode ({ leftSin.release(), rightRemapped.release() });

            expectEquals (node->getAudioNodeProperties().numberOfChannels, 2);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 2, 5.0);
            auto& buffer = testContext->buffer;

            for (int channel : { 0, 1 })
            {
                expectWithinAbsoluteError (buffer.getMagnitude (channel, 0, buffer.getNumSamples()), 1.0f, 0.001f);
                expectWithinAbsoluteError (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()), 0.707f, 0.001f);
            }
        }

        beginTest ("Stereo sin summed to mono");
        {
            // A stero sin node at 0.5 is summed to mono to produce a 1.0 mono sin
            auto node = makeAudioNode<SinAudioNode> (220.0f, 2);
            node = makeGainNode (std::move (node), 0.5f);

            // Merge channe 1 with channel 2
            std::vector<std::pair<int, int>> channelMap;
            channelMap.push_back ({ 0, 0 });
            channelMap.push_back ({ 1, 0 });
            node = makeAudioNode<ChannelMappingAudioNode> (std::move (node), channelMap, true);

            expectEquals (node->getAudioNodeProperties().numberOfChannels, 1);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 1.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.707f, 0.001f);
        }
        
        beginTest ("Twin mono sin summed to mono cancelling");
        {
            // L/R sin with inverted phase that cancel
            auto leftNode = makeAudioNode<SinAudioNode> (220.0f, 1);

            auto rightNode = makeAudioNode<SinAudioNode> (220.0f, 1);
            rightNode = makeAudioNode<FunctionAudioNode> (std::move (rightNode), [] (float s) { return s * -1.0f; });
            rightNode = makeAudioNode<ChannelMappingAudioNode> (std::move (rightNode), makeChannelMap ({ { 0, 1 } }), true);

            auto sumNode = makeSummingAudioNode ({ leftNode.release(), rightNode.release() });

            // Merge channe 1 with channel 2
            auto node = makeAudioNode<ChannelMappingAudioNode> (std::move (sumNode), makeChannelMap ({ { 0, 0 }, { 1, 0 } }), true);

            expectEquals (node->getAudioNodeProperties().numberOfChannels, 1);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
        }
        
        beginTest ("Mono sin duplicated to 6 channel");
        {
            // Create a single mono sin and then copy that to 6 channels
            auto node = makeAudioNode<SinAudioNode> (220.0f, 1);
            node = makeAudioNode<ChannelMappingAudioNode> (std::move (node),
                                                           makeChannelMap ({ { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 5 } }),
                                                           true);

            expectEquals (node->getAudioNodeProperties().numberOfChannels, 6);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 6, 5.0);
            auto& buffer = testContext->buffer;

            for (int channel : { 0, 1, 2, 3, 4, 5 })
            {
                expectWithinAbsoluteError (buffer.getMagnitude (channel, 0, buffer.getNumSamples()), 1.0f, 0.001f);
                expectWithinAbsoluteError (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()), 0.707f, 0.001f);
            }
        }
    }
};

static AudioNodeTests audioNodeTests;

}