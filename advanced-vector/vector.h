#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iterator>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
	RawMemory() = default;

	explicit RawMemory(size_t capacity)
		: buffer_(Allocate(capacity))
		, capacity_(capacity) {
	}

	RawMemory(const RawMemory& other) = delete;
	RawMemory& operator=(const RawMemory& other) = delete;
	RawMemory(RawMemory&& other) noexcept {
		Swap(other);
	}
	RawMemory& operator=(RawMemory&& other) noexcept {
		if (this != &other) {
			Swap(other);
		}
		return *this;
	}

	~RawMemory() {
		Deallocate(buffer_);
	}

	T* operator+(size_t offset) noexcept {
		assert(offset <= capacity_);
		return buffer_ + offset;
	}

	const T* operator+(size_t offset) const noexcept {
		return const_cast<RawMemory&>(*this) + offset;
	}

	const T& operator[](size_t index) const noexcept {
		return const_cast<RawMemory&>(*this)[index];
	}

	T& operator[](size_t index) noexcept {
		assert(index < capacity_);
		return buffer_[index];
	}

	void Swap(RawMemory& other) noexcept {
		std::swap(buffer_, other.buffer_);
		std::swap(capacity_, other.capacity_);
	}

	const T* GetAddress() const noexcept {
		return buffer_;
	}

	T* GetAddress() noexcept {
		return buffer_;
	}

	size_t Capacity() const {
		return capacity_;
	}

private:
	static T* Allocate(size_t n) {
		return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
	}

	static void Deallocate(T* buf) noexcept {
		operator delete(buf);
	}

	T* buffer_ = nullptr;
	size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
	using iterator = T*;
	using const_iterator = const T*;

	iterator begin() noexcept {
		return data_.GetAddress();
	}
	iterator end() noexcept {
		return data_.GetAddress() + size_;
	}
	const_iterator begin() const noexcept {
		return data_.GetAddress();
	}
	const_iterator end() const noexcept {
		return data_.GetAddress() + size_;

	}
	const_iterator cbegin() const noexcept {
		return data_.GetAddress();
	}
	const_iterator cend() const noexcept {
		return data_.GetAddress() + size_;
	}
	Vector() = default;

	explicit Vector(size_t size) :
		data_(size), size_(size) {
		std::uninitialized_value_construct_n(data_.GetAddress(), size_);
	}

	Vector(const Vector& other) :
		data_(other.size_), size_(other.size_) {
		std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
	}

	Vector(Vector&& other) noexcept {
		Swap(other);
	}

	Vector& operator=(const Vector& rhs) {
		if (this != &rhs) {
			if (rhs.size_ > data_.Capacity()) {
				Vector tmp(rhs);
				Swap(tmp);
			}
			else {
				CopyLessVector(rhs);
			}
		}
		return *this;
	}

	Vector& operator=(Vector&& rhs) noexcept {
		if (this != &rhs) {
			Swap(rhs);
		}
		return *this;
	}

	size_t Size() const noexcept {
		return size_;
	}

	size_t Capacity() const noexcept {
		return data_.Capacity();
	}

	const T& operator[](size_t index) const noexcept {
		return const_cast<Vector&>(*this)[index];
	}

	T& operator[](size_t index) noexcept {
		assert(index < size_);
		return data_[index];
	}

	~Vector() {
		std::destroy_n(data_.GetAddress(), size_);
	}

	void Resize(size_t new_size) {
		if (new_size < size_) {
			std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
		}
		else if (new_size > size_) {
			Reserve(new_size);
			std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
		}
		else {
			return;
		}
		size_ = new_size;
	}

	template <typename... Args>
	iterator Emplace(const_iterator pos, Args&&... args) {
		assert(pos >= cbegin() && pos <= cend());
		size_t distance = std::distance(cbegin(), pos);
		if (size_ == Capacity()) {
			size_t new_capacity;
			size_ == 0 ? new_capacity = 1 : new_capacity = size_ * 2;
			RawMemory<T> new_data(new_capacity);
			new(new_data + distance) T(std::forward<Args>(args)...);
			try {
				InitializedNewData(begin(), begin() + distance, new_data.GetAddress()); 
			}
			catch (...) {
				std::destroy_at(new_data.GetAddress() + distance);
				throw;
			}
			try {
				InitializedNewData(begin() + distance, end(), new_data.GetAddress() + distance + 1);
			}
			catch (...) {
				std::destroy_n(new_data.GetAddress(), distance);
				throw;
			}
			std::destroy_n(data_.GetAddress(), size_);
			data_.Swap(new_data);
		}
		else {
			if (size_ != 0) {
				T tmp(std::forward<Args>(args)...);
				try {
					new(data_ + size_) T(std::forward<T>(*(data_.GetAddress() + size_ - 1)));
					std::move_backward(begin() + distance, end() - 1, data_ + size_);
					*(data_.GetAddress() + distance) = std::move(tmp);
				}
				catch (...) {
					std::destroy_at(data_.GetAddress() + size_ + 1);
					throw;
				}
			}
			else {
				new(data_ + distance) T(std::forward<Args>(args)...);
			}
		}
		++size_;
		return data_ + distance;
	}


	iterator Insert(const_iterator pos, const T& value) {
		return Emplace(pos, value);
	}

	iterator Insert(const_iterator pos, T&& value) {
		return Emplace(pos, std::move(value));
	}
	template <typename... Args>
	T& EmplaceBack(Args&&... args) {
		if (size_ == Capacity()) {
			size_t new_capacity;
			size_ == 0 ? new_capacity = 1 : new_capacity = size_ * 2;
			RawMemory<T> new_data(new_capacity);
			new(new_data + size_) T(std::forward<Args>(args)...);
			try {
				InitializedNewData(begin(), begin() + size_, new_data.GetAddress());
			}
			catch (...) {
				std::destroy_at(new_data.GetAddress() + size_);
			}
			std::destroy_n(data_.GetAddress(), size_);
			data_.Swap(new_data);
		}
		else {
			new(data_ + size_) T(std::forward<Args>(args)...);
		}
		++size_;
		return *(data_ + size_ - 1);
	}

	void PushBack(const T& value) {
		EmplaceBack(value);
	}
	void PushBack(T&& value) {
		EmplaceBack(std::move(value));
	}

	void PopBack() noexcept {
		assert(size_ != 0);
		std::destroy_at(data_.GetAddress() + size_ -1);
		--size_;
	}

	void Reserve(size_t capacity) {
		if (capacity > data_.Capacity()) {
			RawMemory<T> new_data(capacity);
			InitializedNewData(data_.GetAddress(), data_.GetAddress() + size_, new_data.GetAddress());
			std::destroy_n(data_.GetAddress(), size_);
			data_.Swap(new_data);
		}
	}

	void Swap(Vector& other) noexcept {
		data_.Swap(other.data_);
		std::swap(size_, other.size_);
	}

	iterator Erase(const_iterator pos) {
		assert(pos >= cbegin() && pos <= cend());
		size_t distance = std::distance(cbegin(),pos);
		std::move(begin() + distance + 1, end(), begin() + distance);
		std::destroy_at(data_.GetAddress() + size_ - 1);
		--size_;
		return data_ + distance;
	}

private:
	RawMemory<T> data_;
	size_t size_ = 0;

	void CopyLessVector(const Vector& other) {
		std::copy(other.begin(), other.begin() + std::min(other.size_, size_), begin());
		if (other.size_ <= size_) {
			std::destroy_n(data_.GetAddress() + other.size_, size_ - other.size_);
		}
		else {
			std::uninitialized_copy_n(other.data_.GetAddress() + size_, other.size_ - size_, data_.GetAddress() + size_);
		}
		size_ = other.size_;
	}

	void InitializedNewData(iterator first, iterator last, iterator d_first) {
		if constexpr (!std::is_copy_constructible_v<T> || std::is_nothrow_move_constructible_v<T>) {
			std::uninitialized_move(first, last, d_first);

		}
		else {
			std::uninitialized_copy(first, last, d_first);
		}
	}
};
