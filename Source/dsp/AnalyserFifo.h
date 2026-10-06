#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace rde
{

/** Single-producer / single-consumer sample FIFO: audio thread pushes, UI pulls. */
class AnalyserFifo
{
public:
    void push (const float* data, int n) noexcept
    {
        const auto scope = fifo.write (std::min (n, fifo.getFreeSpace()));

        if (scope.blockSize1 > 0)
            std::copy (data, data + scope.blockSize1, buffer.begin() + scope.startIndex1);

        if (scope.blockSize2 > 0)
            std::copy (data + scope.blockSize1, data + scope.blockSize1 + scope.blockSize2, buffer.begin() + scope.startIndex2);
    }

    int pull (float* dest, int maxSamples) noexcept
    {
        const auto scope = fifo.read (std::min (maxSamples, fifo.getNumReady()));

        if (scope.blockSize1 > 0)
            std::copy (buffer.begin() + scope.startIndex1, buffer.begin() + scope.startIndex1 + scope.blockSize1, dest);

        if (scope.blockSize2 > 0)
            std::copy (buffer.begin() + scope.startIndex2, buffer.begin() + scope.startIndex2 + scope.blockSize2, dest + scope.blockSize1);

        return scope.blockSize1 + scope.blockSize2;
    }

private:
    static constexpr int kSize = 1 << 15;
    juce::AbstractFifo fifo { kSize };
    std::vector<float> buffer = std::vector<float> ((size_t) kSize, 0.0f);
};

} // namespace rde
