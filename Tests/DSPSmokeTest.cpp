#include <juce_dsp/juce_dsp.h>
#include "../Source/DSP/EchoEngine.h"
#include "../Source/DSP/MicroShift.h"
#include "../Source/DSP/ToneModules.h"

// Runs noise bursts through every echo style/mode and every tone module at extreme settings
// and checks the output stays finite and bounded.
int main()
{
    const double sr = 48000.0;
    const int block = 512, blocks = 2000; // ~21 s
    const double spb = sr * 0.5;          // 120 BPM
    juce::Random rng (1);
    int failures = 0;

    for (int style = 0; style < 6; ++style)
    for (int mode = 0; mode < 3; ++mode)
    {
        arrow::EchoEngine echo;
        echo.prepare (sr, block, 16.5);
        arrow::EchoEngine::Settings s;
        s.style = style; s.mode = mode; s.feedback = 1.1f; s.mix = 1.0f;
        s.saturation = 1.0f; s.wow = 1.0f; s.wowRate = 10.0f; s.diffusion = 1.0f; s.width = 2.0f;
        s.delayMsL = 37.0; s.delayMsR = 55.0; s.reverbOn = true; s.reverbSize = 1.0f; s.reverbMix = 1.0f;

        arrow::MicroShift shift;           shift.prepare (sr, block);
        arrow::FuzzBucket fuzz;            fuzz.prepare (sr);
        arrow::FilterGoblin goblin;        goblin.prepare (sr, block);
        arrow::MotionBox motion;           motion.reset();
        arrow::Squish squish;              squish.prepare (sr, block);

        juce::AudioBuffer<float> buf (2, block);
        float peak = 0.0f;
        double beat = 0.0;
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                    buf.setSample (ch, i, b < 50 ? rng.nextFloat() * 2.0f - 1.0f : 0.0f);

            if (b == 500) { s.delayMsL = 900.0; s.delayMsR = 1500.0; }
            s.freeze = b > 800 && b < 1200;
            s.throwOnly = b > 1200; s.throwing = (b / 50) % 2 == 0;

            echo.process (buf, s);
            shift.process (buf, mode, 1.0f, 1.0f, 150.0f, 1.0f);
            fuzz.process (buf, (style + mode) % 5, 1.0f, (float) mode - 1.0f, 1.0f, 12.0f, true);
            goblin.process (buf, mode, 2000.0f, 1.0f, 1.0f, 1.0f, beat, spb, 1.0);
            motion.process (buf, mode, 1.0f, 1.0f, 1.0f, 1.0f, beat, spb, 0.25);
            squish.process (buf, 1.0f, 10.0f, 1.0f, 1.0f, beat, spb, 1.0);
            beat += block / spb;

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    if (! std::isfinite (v)) { std::printf ("NaN style %d mode %d\n", style, mode); ++failures; b = blocks; break; }
                    peak = std::max (peak, std::abs (v));
                }
        }
        std::printf ("style %d mode %d peak %.2f\n", style, mode, peak);
        if (peak > 30.0f) ++failures;
    }

    std::printf (failures == 0 ? "ALL OK\n" : "FAILURES: %d\n", failures);
    return failures;
}
