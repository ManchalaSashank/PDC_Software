#include "data_manager.h"

void DataManager::addFrame(const PMUFrame& frame)
{
    std::lock_guard<std::mutex> lock(mutex);
    frameBuffer.push_back(frame);
    ++totalFrames;

    if (frameBuffer.size() > maxBufferSize)
        frameBuffer.pop_front();
}

PMUFrame DataManager::getLatest() const
{
    std::lock_guard<std::mutex> lock(mutex);
    if (frameBuffer.empty())
        return PMUFrame();

    return frameBuffer.back();
}

const std::deque<PMUFrame>& DataManager::getAllFrames() const
{
    return frameBuffer;
}

std::deque<PMUFrame> DataManager::getFramesSnapshot() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return frameBuffer;
}

std::size_t DataManager::getTotalFrameCount() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return totalFrames;
}

// Added method for history window
const std::deque<PMUFrame>& DataManager::getHistory() const
{
    return frameBuffer;
}

void DataManager::clear()
{
    std::lock_guard<std::mutex> lock(mutex);
    frameBuffer.clear();
    totalFrames = 0;
}
