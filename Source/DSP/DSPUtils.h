#pragma once

#include <cmath>

namespace arrow
{
    // Linear-interpolated read from a circular buffer.
    inline float readInterpolated (const float* data, int size, double pos)
    {
        pos = std::fmod (pos, (double) size);
        if (pos < 0.0)
            pos += size;

        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % size;
        const float frac = (float) (pos - i0);
        return data[i0] + frac * (data[i1] - data[i0]);
    }

    // Dry stays at full level up to 50% mix, wet reaches full level at 50% mix.
    struct BlendGains
    {
        float dry, wet;
    };

    inline BlendGains blendGains (float mix)
    {
        return { mix <= 0.5f ? 1.0f : 2.0f * (1.0f - mix),
                 mix >= 0.5f ? 1.0f : 2.0f * mix };
    }

    // Transparent below 0.8, smoothly limits to +/-1 above.
    inline float softLimit (float x)
    {
        const float a = std::abs (x);
        if (a < 0.8f)
            return x;

        const float limited = 0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f);
        return x < 0.0f ? -limited : limited;
    }
}
