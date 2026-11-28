#pragma once

enum EventType { SPAWN_REINFORCEMENT, END_ALARM };

struct ScheduledEvent {
    EventType type;
    double time;
    unsigned int sequence;
};

class EventHeap {
public:
    static constexpr int Capacity = 32;

    bool IsEmpty() const { return size_ == 0; }
    int Size() const { return size_; }

    bool Insert(EventType type, double time) {
        if (size_ == Capacity) {
            return false;
        }
        const int index = size_++;
        events_[index] = {type, time, nextSequence_++};
        HeapifyUp(index);
        return true;
    }

    bool Peek(ScheduledEvent& event) const {
        if (IsEmpty()) {
            return false;
        }
        event = events_[0];
        return true;
    }

    bool ExtractMin(ScheduledEvent& event) {
        if (!Peek(event)) {
            return false;
        }
        --size_;
        if (!IsEmpty()) {
            events_[0] = events_[size_];
            HeapifyDown(0);
        }
        return true;
    }

    void Clear() {
        size_ = 0;
        nextSequence_ = 0;
    }

private:
    static bool Earlier(ScheduledEvent a, ScheduledEvent b) {
        return a.time < b.time || (a.time == b.time && a.sequence < b.sequence);
    }

    void Swap(int a, int b) {
        const ScheduledEvent temp = events_[a];
        events_[a] = events_[b];
        events_[b] = temp;
    }

    void HeapifyUp(int index) {
        while (index > 0) {
            const int parent = (index - 1) / 2;
            if (!Earlier(events_[index], events_[parent])) {
                return;
            }
            Swap(index, parent);
            index = parent;
        }
    }

    void HeapifyDown(int index) {
        while (2 * index + 1 < size_) {
            int child = 2 * index + 1;
            const int right = child + 1;
            if (right < size_ && Earlier(events_[right], events_[child])) {
                child = right;
            }
            if (!Earlier(events_[child], events_[index])) {
                return;
            }
            Swap(index, child);
            index = child;
        }
    }

    ScheduledEvent events_[Capacity]{};
    int size_ = 0;
    unsigned int nextSequence_ = 0;
};
