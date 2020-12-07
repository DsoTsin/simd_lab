#pragma once

#include <math.h>
#include <memory>
#include <vector>
#include <assert.h>
#define FORCEINLINE __forceinline
#define check(x) assert(x)

#ifndef STATS
#define STATS 1
#endif

#ifdef _MSC_VER
extern "C" void *_InterlockedCompareExchangePointer(void *volatile *Destination,
                                                    void *Exchange,
                                                    void *Comparand);
#pragma intrinsic(_InterlockedCompareExchangePointer)
#else

#endif

namespace __intrin__ {
inline bool atomicCASPtr(void **Destination, void *NewValue,
                             void *OldValue) {
#if defined(_MSC_VER)
  return _InterlockedCompareExchangePointer((void *volatile *)Destination,
                                            NewValue, OldValue) == OldValue;
#else

#endif
}

extern "C" void atomic_increment64(size_t* ptr); // defined in asm

}

#include "ue_math.h"
#include "RefCounted.h"
#include "Threading.h"

#include "concurrentqueue.h"
#include "vectorlist.hpp"

struct FVector {
  union {
    struct {
      float X;
      float Y;
      float Z;
    };
    float V[3];
  };
};

__declspec(align(16)) struct FPlane : public FVector {
public:
  /** The w-component. */
  float W;
};

struct FBoxSphereBounds {
  /** Holds the origin of the bounding box and sphere. */
  FVector Origin;

  /** Holds the extent of the bounding box. */
  FVector BoxExtent;

  /** Holds the radius of the bounding sphere. */
  float SphereRadius;
};

template <int N, typename T = FPlane> class inline_alloc {
public:
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef T *pointer;
  typedef const T *const_pointer;
  typedef T &reference;
  typedef const T &const_reference;
  typedef T value_type;

  template <class U> struct rebind { typedef inline_alloc<N, U> other; };

  pointer allocate(size_type n, const void * = 0) { return storage; }

  void deallocate(void *p, size_type) {}

  pointer address(reference x) const { return &x; }
  const_pointer address(const_reference x) const { return &x; }
  inline_alloc<N, T> &operator=(const inline_alloc &) { return *this; }

  void construct(pointer p, const T &val) { new ((T *)p) T(val); }
  void destroy(pointer p) { p->~T(); }

  size_type max_size() const { return N * sizeof(T); }

  template <class U> inline_alloc(const inline_alloc<N, U> &) {}

  template <class U> inline_alloc &operator=(const inline_alloc<N, U> &) {
    return *this;
  }

  inline_alloc() {}
  ~inline_alloc() {}

private:
  T storage[N];
};

struct FConvexVolume {
public:
  typedef std::vector<FPlane, inline_alloc<6>> FPlaneArray;
  typedef std::vector<FPlane, inline_alloc<8>> FPermutedPlaneArray;

  FPlaneArray Planes;
  /** This is the set of planes pre-permuted to SSE/Altivec form */
  FPermutedPlaneArray PermutedPlanes;

  FConvexVolume() {
    //		int32 N = 5;
  }

  bool IntersectBox(const FVector &Origin, const FVector &Extent) const;
  bool IntersectBox(const FVector &Origin, const FVector &Extent,
                    bool &bOutFullyContained) const;
  bool IntersectSphere(const FVector &Origin, const float &Radius) const;
  bool IntersectSphere(const FVector &Origin, const float &Radius,
                       bool &bOutFullyContained) const;
};

template <typename T> class TArray : public std::vector<T> {
public:
  int32 Add(const T &Elem) {
    int32 ENum = this->size();
    push_back(Elem);
    return ENum;
  }
  int32 Num() const { return (int32)this->size(); }
};

template <typename T> class LockFreeQueue {
public:
  LockFreeQueue() : m_Size(0), m_Head(new Node()), m_Tail(m_Head) {}

  ~LockFreeQueue() {
    while (DequeueAndDelete())
      ;
    delete m_Head;
  }

  bool IsEmpty() const { return m_Head->m_Next == nullptr; }

  void Enqueue(const T &v) {
    Node *node = new Node(v);
    while (1) {
      Node *last = m_Tail;
      Node *next = last->m_Next;
      if (last != m_Tail) {
        continue;
      }
      if (next == nullptr) {
        if (__intrin__::atomicCASPtr((void **)&last->m_Next, node,
                                             next)) {
          __intrin__::atomicCASPtr((void **)&m_Tail, node, last);
          ++m_Size;
          return;
        }
      } else {
        __intrin__::atomicCASPtr((void **)&m_Tail, next, last);
      }
    }
  }

  bool Dequeue(T &Val) { // it returns false if there is nothing to deque.
    while (1) {
      Node *first = m_Head;
      Node *last = m_Tail;
      Node *next = first->m_Next;

      if (first != m_Head) {
        continue;
      }

      if (first == last) {
        if (next == nullptr) {
          return false;
        }
        __intrin__::atomicCASPtr((void **)&m_Tail, next, last);
      } else {
        T result = next->m_Value;
        if (__intrin__::atomicCASPtr((void **)&m_Head, next, first)) {
          delete first;
          Val = result;
          return true;
        }
      }
    }
    return false;
  }

  bool DequeueAndDelete() { // it destroys dequed item. but fast.
    while (1) {
      Node *first = m_Head;
      Node *last = m_Tail;
      Node *next = first->m_Next;

      if (first != m_Head) {
        continue;
      }

      if (first == last) {
        if (next == nullptr) {
          return false;
        }
        __intrin__::atomicCASPtr((void **)&m_Tail, next, last);
      } else {
        if (__intrin__::atomicCASPtr((void **)&m_Head, next, first)) {
          delete first;
          return true;
        }
      }
    }
  }

  LockFreeQueue(const LockFreeQueue &&) = delete;
  LockFreeQueue(const LockFreeQueue &) = delete;
  LockFreeQueue &operator=(const LockFreeQueue &) = delete;

private:
  class Node {
  public:
    const T m_Value;
    Node *m_Next;
    Node(const T &v) : m_Value(v), m_Next(nullptr) {}
    Node() : m_Value(), m_Next(nullptr){};

  private:
    Node(const Node &) = delete;
    Node &operator=(const Node &) = delete;
  };

  volatile int m_Size;

  Node *m_Head, *m_Tail;
};