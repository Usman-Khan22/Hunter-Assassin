#pragma once

class DoorHash {
public:
    DoorHash() = default;
    ~DoorHash() { Clear(); }
    DoorHash(const DoorHash&) = delete;
    DoorHash& operator=(const DoorHash&) = delete;

    bool Insert(int key, int doorIndex) {
        if (key < 0 || doorIndex < 0 || Find(key) != -1) {
            return false;
        }
        const int bucket = key % BucketCount;
        buckets_[bucket] = new Node{key, doorIndex, buckets_[bucket]};
        return true;
    }

    int Find(int key) const {
        if (key < 0) {
            return -1;
        }
        const Node* node = buckets_[key % BucketCount];
        while (node != nullptr) {
            if (node->key == key) {
                return node->doorIndex;
            }
            node = node->next;
        }
        return -1;
    }

    void Clear() {
        for (int bucket = 0; bucket < BucketCount; ++bucket) {
            while (buckets_[bucket] != nullptr) {
                Node* node = buckets_[bucket];
                buckets_[bucket] = node->next;
                delete node;
            }
        }
    }

private:
    struct Node {
        int key;
        int doorIndex;
        Node* next;
    };
    static constexpr int BucketCount = 17;
    Node* buckets_[BucketCount]{};
};
