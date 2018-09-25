#ifndef TRACKINDEX
#define TRACKINDEX

#include <cstdint>

class TrackIndex
{
public:
    TrackIndex() : m_trackIndex(0)
    { }

    TrackIndex(uint8_t track, uint8_t index) : m_trackIndex((uint16_t(track) << 8) | index)
    { }

    TrackIndex(uint16_t value) : m_trackIndex(value)
    { }

    TrackIndex(const TrackIndex& other) : m_trackIndex(other.m_trackIndex)
    { }

    uint8_t track() const
    {
        return m_trackIndex >> 8;
    }

    void setTrack(uint8_t track)
    {
        m_trackIndex &= 0xFF;
        m_trackIndex |= uint16_t(track) << 8;
    }

    uint8_t index() const
    {
        return m_trackIndex & 0xFF;
    }

    void setIndex(uint8_t index)
    {
        m_trackIndex &= 0xFF00;
        m_trackIndex |= index;
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
        return m_trackIndex < other.m_trackIndex;
    }

    static uint16_t makeTrackIndex(uint8_t track, uint8_t index)
    {
        return (uint16_t(track) << 8) | index;
    }

protected:
    uint16_t m_trackIndex;
};

#endif // TRACKINDEX

