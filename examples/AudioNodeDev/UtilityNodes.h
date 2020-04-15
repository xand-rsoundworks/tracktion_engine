/*
  ==============================================================================

    UtilityNodes.h
    Created: 15 Apr 2020 10:43:35am
    Author:  David Rowland

  ==============================================================================
*/

#pragma once

//==============================================================================
//==============================================================================
class LatencyAudioNode  : public AudioNode
{
public:
    LatencyAudioNode (std::unique_ptr<AudioNode> inputNode, int numSamplesToDelay)
        : input (std::move (inputNode)), latencyNumSamples (numSamplesToDelay)
    {
    }
    
    AudioNodeProperties getAudioNodeProperties() override
    {
        auto props = input->getAudioNodeProperties();
        props.latencyNumSamples = std::max (props.latencyNumSamples, latencyNumSamples);
        
        return props;
    }
    
    std::vector<AudioNode*> getAllInputNodes() override
    {
        std::vector<AudioNode*> inputNodes;
        inputNodes.push_back (input.get());
        
        auto nodeInputs = input->getAllInputNodes();
        inputNodes.insert (inputNodes.end(), nodeInputs.begin(), nodeInputs.end());

        return inputNodes;
    }
    
    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        fifo.setSize (getAudioNodeProperties().numberOfChannels, latencyNumSamples + info.blockSize + 1);
        fifo.writeSilence (latencyNumSamples);
        jassert (fifo.getNumReady() == latencyNumSamples);
    }
    
    void process (const ProcessContext& pc) override
    {
        auto& outputBlock = pc.buffers.audio;
        auto inputBuffer = input->getProcessedOutput().audio;
        jassert (fifo.getNumChannels() == inputBuffer.getNumChannels());
        fifo.write (inputBuffer);
        
        jassert (fifo.getNumReady() >= outputBlock.getNumSamples());
        jassert (inputBuffer.getNumSamples() == outputBlock.getNumSamples());

        fifo.readAdding (outputBlock);
        
        //TODO: MIDI
    }
    
private:
    std::unique_ptr<AudioNode> input;
    const int latencyNumSamples;
    tracktion_engine::AudioFifo fifo { 1, 32 };
};


//==============================================================================
//==============================================================================
/**
    An AudioNode which sums together the multiple inputs adding additional latency
    to provide a coherent output.
*/
class SummingAudioNode : public AudioNode
{
public:
    SummingAudioNode (std::vector<std::unique_ptr<AudioNode>> inputs)
        : nodes (std::move (inputs))
    {
        const int maxLatency = getAudioNodeProperties().latencyNumSamples;
        
        for (auto& node : nodes)
        {
            const int nodeLatency = node->getAudioNodeProperties().latencyNumSamples;
            const int latencyToAdd = maxLatency - nodeLatency;
            
            if (latencyToAdd == 0)
                continue;
            
            std::unique_ptr<AudioNode> latencyNode = std::make_unique<LatencyAudioNode> (std::move (node), latencyToAdd);
            node.swap (latencyNode);
        }
    }
    
    AudioNodeProperties getAudioNodeProperties() override
    {
        AudioNodeProperties props;
        props.hasAudio = false;
        props.hasMidi = false;
        props.numberOfChannels = 0;

        for (auto& node : nodes)
        {
            auto nodeProps = node->getAudioNodeProperties();
            props.hasAudio = props.hasAudio | nodeProps.hasAudio;
            props.hasMidi = props.hasMidi | nodeProps.hasMidi;
            props.numberOfChannels = std::max (props.numberOfChannels, nodeProps.numberOfChannels);
            props.latencyNumSamples = std::max (props.latencyNumSamples, nodeProps.latencyNumSamples);
        }

        return props;
    }
    
    std::vector<AudioNode*> getAllInputNodes() override
    {
        std::vector<AudioNode*> inputNodes;
        
        for (auto& node : nodes)
        {
            inputNodes.push_back (node.get());
            
            auto nodeInputs = node->getAllInputNodes();
            inputNodes.insert (inputNodes.end(), nodeInputs.begin(), nodeInputs.end());
        }

        return inputNodes;
    }

    bool isReadyToProcess() override
    {
        for (auto& node : nodes)
            if (! node->hasProcessed())
                return false;
        
        return true;
    }
    
    void process (const ProcessContext& pc) override
    {
        // Get each of the inputs and add them to dest
        for (auto& node : nodes)
        {
            pc.buffers.audio.add (node->getProcessedOutput().audio);
            //TODO:  MIDI
        }
    }

private:
    std::vector<std::unique_ptr<AudioNode>> nodes;
};


/** Creates a SummingAudioNode from a number of AudioNodes. */
std::unique_ptr<SummingAudioNode> makeSummingAudioNode (std::initializer_list<AudioNode*> nodes)
{
    std::vector<std::unique_ptr<AudioNode>> nodeVector;
    
    for (auto node : nodes)
        nodeVector.push_back (std::unique_ptr<AudioNode> (node));
        
    return std::make_unique<SummingAudioNode> (std::move (nodeVector));
}
