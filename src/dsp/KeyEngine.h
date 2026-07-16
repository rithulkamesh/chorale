#pragma once

// Krumhansl-Schmuckler key finding on a decaying chroma histogram (built from
// the tracked f0, since input is monophonic), plus the diatonic harmony-target
// calculator.
class KeyEngine
{
public:
    // Scale index order (also used by the plugin's Scale parameter, offset by
    // one for "Auto"): 0 Major, 1 Minor, 2 Dorian, 3 Phrygian, 4 Lydian,
    // 5 Mixolydian, 6 Locrian, 7 Chromatic.
    static constexpr int kChromatic = 7;

    void reset();
    void addObservation (float f0, float weight); // voiced pitch estimate per hop
    int rootPc() const { return root; }
    bool isMinor() const { return minorKey; }

    // Nearest scale degree to noteMidi, moved by degreeOffset scale steps.
    static int harmonyTarget (int noteMidi, int rootPc, int scaleIdx, int degreeOffset);

private:
    void detect();

    float chroma[12] = {};
    int root = 0;
    bool minorKey = false;
    int sinceDetect = 0;
};
