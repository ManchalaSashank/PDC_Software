#pragma once
#include <cstddef>
#include <deque>
#include <mutex>
#include "data_parser.h"

class DataManager
{
private:
    std::deque<PMUFrame> frameBuffer;
    mutable std::mutex mutex;
    std::size_t totalFrames = 0;
    static constexpr std::size_t maxBufferSize = 2000;

public:
    void addFrame(const PMUFrame& frame);

    PMUFrame getLatest() const;

    const std::deque<PMUFrame>& getAllFrames() const;
    std::deque<PMUFrame> getFramesSnapshot() const;
    std::size_t getTotalFrameCount() const;

    // Added for HistoryWindow compatibility
    const std::deque<PMUFrame>& getHistory() const;

    void clear();
};
