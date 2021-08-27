#ifndef __Templates_h__
#define __Templates_h__
#pragma once

#ifdef USE_UNREAL_TEMPLATES

#else

#include <vector>
template <class T>
class TArray {
public:
	typedef typename std::vector<T>::iterator iterator;
	typedef typename std::vector<T>::const_iterator const_iterator;

	TArray() {}

	int Num() const { return static_cast<int>(data_.size()); }

	const T* GetData() const { return data_.data(); }
	T* GetData() { return data_.data(); }

	void Resize(int InNum) { data_.resize(InNum); }
	void Reserve(int InNum) { data_.reserve(InNum); }
	void Empty() { data_.clear(); }

	void Add(const T& InElement) { data_.push_back(InElement); }

	T& Last() { return data_.back(); }

	const T& operator[](size_t Index) const { return data_.at(Index); }
	T& operator[](size_t Index) { return data_.at(Index); }

	iterator begin() { return data_.begin(); }
	iterator end() { return data_.end(); }

private:
	std::vector<T> data_;
};
#endif // USE_UNREAL_TEMPLATES


#endif