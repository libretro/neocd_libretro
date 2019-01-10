#ifndef TRACKINDEX
#define TRACKINDEX

#include <cstdint>
#include <utility>

// Class used to store both track and index.

class TrackIndex
{
public:
    TrackIndex(uint8_t track = 0, uint8_t index = 0) : 
        m_trackIndex(std::make_pair(track, index))
    { }

    TrackIndex(const TrackIndex& other) : 
        m_trackIndex(other.m_trackIndex)
    { }

    uint8_t track() const
    {
        return m_trackIndex.first;
    }

    void setTrack(uint8_t track)
    {
        m_trackIndex.first = track;
    }

    uint8_t index() const
    {
        return m_trackIndex.second;
    }

    void setIndex(uint8_t index)
    {
        m_trackIndex.second = index;
    }

    bool operator==(const TrackIndex& other) const
    {
        return m_trackIndex == other.m_trackIndex;
    }

    TrackIndex& operator=(const TrackIndex& other)
    {
        m_trackIndex = other.m_trackIndex;
        return *this;
    }

    bool operator<(const TrackIndex& other) const
    {
        if (m_trackIndex.first == other.m_trackIndex.first)
            return m_trackIndex.second < other.m_trackIndex.second;

        return m_trackIndex.first < other.m_trackIndex.first;
    }

protected:
    std::pair<uint8_t, uint8_t> m_trackIndex;
};

#endif // TRACKINDEX
