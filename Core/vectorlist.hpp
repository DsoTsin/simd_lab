#pragma once

template <typename T> class TLinkVector {
public:
  // this is intrusive
  struct Vector {
    T *data;
    size_t num;
    size_t capacity;

    Vector();
    ~Vector();

    size_t add(const T &elem);
    void reserve(size_t capacity);

    void unlink();

  private:
    Vector *next;
    Vector *prev;
    TLinkVector *list;
    friend class TLinkVector<T>;
  };

  struct iterator {};
  struct vectorIter {
    Vector *vec;
    vectorIter(Vector *inVec) : vec(inVec) {}

    operator bool() const { return vec != nullptr; }

    vectorIter &operator++() {
      vec = vec->next;
      return this;
    }

    const vectorIter &operator++(int) {
      vec = vec->next;
      return *this;
    }

    Vector &operator*() { return *vec; }

    const Vector &operator*() const { return *vec; }
  };

  TLinkVector();
  ~TLinkVector();

  void reset();
  Vector *newVector();

  size_t num() const { return num_; }
  size_t accum() const { return accum_; }

#if STATS
  size_t resize_count() const { return resize_count_; }
#endif

  vectorIter begin() const { return vectorIter(head_->next); }

private:
  Vector *head_;
  Vector *tail_;

  size_t num_;
  size_t accum_;

#if STATS
  size_t resize_count_;
#endif
};

template <typename T>
inline TLinkVector<T>::Vector::Vector()
    : data(nullptr), num(0), capacity(0), next(nullptr), prev(nullptr),
      list(nullptr) {}

template <typename T> inline TLinkVector<T>::Vector::~Vector() {
  if (data) {
    for (size_t i = 0; i < num; i++) {
      data[i].~T();
    }
    ::free(data);
    data = nullptr;
  }
}

template <typename T> inline size_t TLinkVector<T>::Vector::add(const T &elem) {
  size_t cur = num;
  if (num + 1 > capacity) {
#if STATS
    __intrin__::atomic_increment64(&list->resize_count_);
#endif
    capacity = size_t(1.3 * capacity) + 1;
    T *newData = (T *)malloc(capacity * sizeof(T));
    ::memcpy(newData, data, num * sizeof(T));
    // construct remain
    for (size_t i = num; i < capacity; i++) {
      new (newData + i) T;
    }
    ::free(data);
    data = newData;
  }

  data[cur] = elem;
  num++;
  __intrin__::atomic_increment64(&list->accum_);
  return cur;
}

template <typename T>
inline void TLinkVector<T>::Vector::reserve(size_t inCapacity) {
  if (inCapacity > capacity) {
    capacity = inCapacity;
    if (data) {
      ::free(data);
    }
    data = (T *)::malloc(capacity * sizeof(T));
    memset(data, 0, capacity * sizeof(T));
  }
}

// need CAS op
template <typename T> inline void TLinkVector<T>::Vector::unlink() {
  using node = typename TLinkVector<T>::Vector;
  node *p = prev;
  node *n = next;
  // relink
  if (p) {
    p->next = n;
  }
  if (n) {
    n->prev = p;
  }
  // set null
  prev = nullptr;
  next = nullptr;
  if (list) {
    if (list->head_ == this) {
      list->head_ = n;
    }
    if (list->tail_ == this) {
      list->tail_ = n;
    }
    list->num_--;
    list->accum_ -= num;
  }
  list = nullptr;
}

template <typename T>
inline TLinkVector<T>::TLinkVector()
    : head_(nullptr), tail_(nullptr), num_(0), accum_(0)
#if STATS
    , resize_count_(0)
#endif
{
  head_ = new TLinkVector<T>::Vector; // add dummy head
  tail_ = head_;
}

template <typename T> inline TLinkVector<T>::~TLinkVector() {
  auto ptr = head_;
  head_ = nullptr;
  tail_ = nullptr;
  while (ptr) {
    auto next = ptr->next;
    ptr->unlink();
    delete ptr;
    ptr = next;
  }
}

// template <typename T> inline void TLinkVector<T>::add(const T &elem) {}

// template <typename T> inline void TLinkVector<T>::add(Vector &v) {}

template <typename T> inline void TLinkVector<T>::reset() {
  auto ptr = head_;
  // head_ = nullptr;
  // tail_ = nullptr;
  head_ = new TLinkVector<T>::Vector; // add dummy head
  tail_ = head_;

  while (ptr) {
    auto next = ptr->next;
    ptr->unlink();
    delete ptr;
    ptr = next;
  }
  accum_ = 0;
  num_ = 0;

#if STATS
  resize_count_ = 0;
#endif
}

template <typename T>
inline typename TLinkVector<T>::Vector *TLinkVector<T>::newVector() {
  auto vn = new TLinkVector<T>::Vector;
  vn->list = this;
  // CAS op?
  if (!head_) {
    head_ = vn;
    tail_ = head_;
  } else {
    assert(tail_);
    vn->prev = tail_;
    vn->next = nullptr;
    tail_->next = vn;
    tail_ = vn;
  }
  num_++;
  return tail_;
}
