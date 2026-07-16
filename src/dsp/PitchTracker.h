#pragma once

struct PitchEstimate
{
    float f0 = 0.0f;         // Hz, valid only when voiced
    float confidence = 0.0f; // 0..1
    bool voiced = false;
};

// Frame-based monophonic pitch tracker. CREPE/ONNX drops in behind this same
// interface later; the engine owns buffering and calls analyze() once per hop.
class PitchTracker
{
public:
    virtual ~PitchTracker() = default;
    virtual void prepare (double sampleRate) = 0;
    virtual int frameSize() const = 0;
    virtual PitchEstimate analyze (const float* frame) = 0;
};
