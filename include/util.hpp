#pragma once

extern "C" {
	#include <stddef.h>
	#include <stdint.h>
	#include <stdlib.h>
	int puts(const char *);
}

// for things we need but can't have... yet...
// these are not identical to the standard ones and will have unexpected behavior

#ifdef DEBUG
void printBacktrace();
#ifdef CAN_EXIT
#pragma clang diagnostic ignored "-Wformat-security"
template <typename ...Types>
[[noreturn]] static void fatalf(const char *msg, Types... values) {
	__builtin_printf(msg, values...);
	printBacktrace();
	exit(0);
}
#else
#pragma clang diagnostic ignored "-Wformat-security"
template <typename ...Types>
static void fatalf(const char *msg, Types... values) {
	__builtin_printf(msg, values...);
	printBacktrace();
}
#endif
#endif

template <typename T>
class UniquePtr {

	T *ptr;

	public:
		UniquePtr() = delete;
		UniquePtr(decltype(nullptr)) : ptr(nullptr) {}
		UniquePtr(T *ptr) : ptr(ptr) {}
		UniquePtr(const UniquePtr &rhs) = delete;
		UniquePtr &operator=(const UniquePtr &rhs) = delete;
		UniquePtr(UniquePtr &&rhs) {
			delete ptr;
			ptr = rhs.ptr;
			rhs.ptr = nullptr;
		}
		UniquePtr &operator=(UniquePtr &&rhs) {
			delete ptr;
			ptr = rhs.ptr;
			rhs.ptr = nullptr;
			return *this;
		}
		~UniquePtr() { delete ptr;}

		explicit operator bool() const {
			return ptr != nullptr;
		}

		T &operator*() {
			#ifdef DEBUG
			if (ptr == nullptr) [[unlikely]] {
				fatalf("nullptr dereference\n");
			}
			#endif
			return *ptr;
		}

		const T &operator*() const {
			#ifdef DEBUG
			if (ptr == nullptr) [[unlikely]] {
				fatalf("nullptr dereference\n");
			}
			#endif
			return *ptr;
		}

		T *operator->() const {
			#ifdef DEBUG
			if (ptr == nullptr) [[unlikely]] {
				fatalf("nullptr dereference\n");
			}
			#endif
			return ptr;
		}

		bool operator==(decltype(nullptr)) const { return ptr == nullptr; }
		bool operator!=(decltype(nullptr)) const { return ptr != nullptr; }

		T *get() const {
			return ptr;
		}

		T *release() {
			T *tmp = ptr;
			ptr = nullptr;
			return tmp;
		}
};

template <typename T>
class UniquePtr<T[]> {

	T *ptr;

	public:
		UniquePtr() = delete;
		UniquePtr(decltype(nullptr)) : ptr(nullptr) {}
		UniquePtr(T *ptr) : ptr(ptr) {}
		UniquePtr(const UniquePtr &rhs) = delete;
		UniquePtr &operator=(const UniquePtr &rhs) = delete;
		UniquePtr(UniquePtr &&rhs) {
			delete[] ptr;
			ptr = rhs.ptr;
			rhs.ptr = nullptr;
		}
		UniquePtr &operator=(UniquePtr &&rhs) {
			delete[] ptr;
			ptr = rhs.ptr;
			rhs.ptr = nullptr;
			return *this;
		}
		~UniquePtr() { delete[] ptr; }

		explicit operator bool() const {
			return ptr;
		}

		T &operator[](size_t i ) const {
			#ifdef DEBUG
			if (ptr == nullptr) [[unlikely]] {
				fatalf("nullptr dereference\n");
			}
			#endif
			return ptr[i];
		}

		bool operator==(decltype(nullptr)) const { return ptr == nullptr; }
		bool operator!=(decltype(nullptr)) const { return ptr != nullptr; }

		T *get() const {
			#ifdef DEBUG
			if (ptr == nullptr) [[unlikely]] {
				fatalf("nullptr dereference\n");
			}
			#endif
			return ptr;
		}

		T *release() {
			T *tmp = ptr;
			ptr = nullptr;
			return tmp;
		}
};


class StringView {
	const char * str;
	size_t size;

	public:
		StringView(decltype(nullptr)) : str(nullptr), size(0) {}

		StringView(const char *str) : str(str), size(__builtin_strlen(str)) {}

		constexpr StringView(const char *str, unsigned long length) : str(str), size(length) {}

		StringView(const StringView &) = default;
		StringView &operator=(const StringView &) = default;
		StringView(StringView &&) = default;
		StringView &operator=(StringView &&) = default;

		explicit operator bool() const {
			return str;
		}

		constexpr size_t length() const {
			return size;
		}

		operator const char*() const {
			return str;
		}

		const char *c_str() const {
			return str;
		}

		constexpr bool operator==(decltype(nullptr)) const {
			return str == nullptr;
		}

		constexpr bool operator==(const StringView &rhs) const {
			if (size != rhs.size) {
				return false;
			}
			if (str == rhs.str) {
				return true;
			}
			if (str == nullptr || rhs.str == nullptr) {
				return false;
			}
			return __builtin_memcmp(str, rhs.str, size) == 0;
		}

		bool contains(const char *needle) const {
			return __builtin_strstr(str, needle);
		}

		constexpr bool endswith(const StringView &view) const {
			const size_t sz = view.length();
			const char *buf = view.c_str();
			return sz > size ? false : __builtin_memcmp(str + (size - sz), buf, sz) == 0;
		}

		constexpr bool startswith(const StringView &view) const {
			const size_t sz = view.length();
			const char *buf = view.c_str();
			return sz > size ? false : __builtin_memcmp(str, buf, sz) == 0;
		}
};

constexpr StringView operator"" _sv(const char *str, unsigned long len) { return {str, len}; }

class String {

	static constexpr size_t SSO_MAX = 16;

	union StringBuf {
		char *ptr;
		char buf[SSO_MAX];
	};

	StringBuf data;
	size_t size;
	size_t capacity;

	static constexpr size_t align(size_t x) {
		auto r = x % SSO_MAX;
    	return r ? x + (SSO_MAX - r) : x;
	}

	bool is_sso() const { return capacity == SSO_MAX; }

	void grow(size_t target) {
		const size_t newCapacity = align(target);
		if (is_sso()) {
			char *tmp = new char[newCapacity];
			__builtin_memcpy(tmp, data.buf, size);
			tmp[size] = '\0';
			data.ptr = tmp;
		} else {
			char *tmp = new char[newCapacity];
			__builtin_memcpy(tmp, data.ptr, size);
			tmp[size] = '\0';
			delete[] data.ptr;
			data.ptr = tmp;
		}
		capacity = newCapacity;
	}

	char *buffer() {
		return is_sso() ? data.buf : data.ptr;
	}

	const char *buffer() const {
		return is_sso() ? data.buf : data.ptr;
	}

	public:
		explicit String() : data(), size(0), capacity(SSO_MAX) { data.buf[0] = '\0'; }
		String(const StringView &view) : size(view.length()), capacity(align(view.length()+1)) {
			if (is_sso()) {
				__builtin_memcpy(data.buf, view.c_str(), size);
				data.buf[size] = '\0';
			} else {
				data.ptr = new char[capacity];
				__builtin_memcpy(data.ptr, view.c_str(), size);
				data.ptr[size] = '\0';
			}
		}
		String(const String &rhs) : size(rhs.size), capacity(rhs.capacity) {
			if (is_sso()) {
				__builtin_memcpy(data.buf, rhs.c_str(), size+1);
				capacity = SSO_MAX;
			} else {
				data.ptr = new char[rhs.capacity];
				__builtin_memcpy(data.ptr, rhs.c_str(), size+1);
			}
		}
		String &operator=(const String &rhs) {
			size = rhs.size;
			capacity = rhs.capacity;
			if (is_sso()) {
				__builtin_memcpy(data.buf, rhs.c_str(), size+1);
				capacity = SSO_MAX;
			} else {
				data.ptr = new char[rhs.capacity];
				__builtin_memcpy(data.ptr, rhs.c_str(), size+1);
			}
			return *this;
		}
		String(String &&rhs) : size(rhs.size), capacity(rhs.capacity) {
			if (is_sso()) {
				__builtin_memcpy(data.buf, rhs.c_str(), size+1);
				capacity = SSO_MAX;
			} else {
				data.ptr = new char[rhs.capacity];
				__builtin_memcpy(data.ptr, rhs.c_str(), size+1);
				rhs.data.ptr = nullptr;
			}
		}
		String &operator=(String &&rhs) {
			size = rhs.size;
			capacity = rhs.capacity;
			if (is_sso()) {
				__builtin_memcpy(data.buf, rhs.c_str(), size+1);
				capacity = SSO_MAX;
			} else {
				data.ptr = new char[rhs.capacity];
				__builtin_memcpy(data.ptr, rhs.c_str(), size+1);
				rhs.data.ptr = nullptr;
			}
			return *this;
		}
		~String() {
			if (!is_sso()) {
				delete[] data.ptr;
			}
		}

		String &operator+=(const String &rhs) {
			const size_t newsz = size + rhs.length();
			if (newsz >= capacity) {
				grow(newsz + 1); // an additional character must be reserved for '\0'
			}
			char *__restrict ptr = buffer();
			__builtin_memcpy(ptr + size, rhs.c_str(), rhs.length());
			size = newsz;
			ptr[size] = '\0';
			return *this;
		}

		String &operator+=(const char c) {
			const size_t newsz = size + 1;
			if (newsz >= capacity) {
				grow(newsz + 1); // an additional character must be reserved for '\0'
			}
			char *__restrict ptr = buffer();
			ptr[size++] = c;
			ptr[size] = '\0';
			return *this;
		}

		bool operator==(const StringView &rhs) const {
			if (size != rhs.length()) {
				return false;
			}
			return __builtin_memcmp(buffer(), rhs.c_str(), size) == 0;
		}

		const char *c_str() const {
			return is_sso() ? data.buf : data.ptr;
		}

		size_t length() const {
			return size;
		}

		bool contains(const char *needle) const {
			return __builtin_strstr(buffer(), needle);
		}

		bool endswith(const StringView &view) const {
			const size_t sz = view.length();
			const char *buf = view.c_str();
			return sz > size ? false : __builtin_memcmp(buffer() + (size - sz), buf, sz) == 0;
		}

		bool startswith(const StringView &view) const {
			const size_t sz = view.length();
			const char *buf = view.c_str();
			return sz > size ? false : __builtin_memcmp(buffer(), buf, sz) == 0;
		}

		operator StringView() const {
			return {buffer(), size};
		}

		StringView subview(size_t from) const {
			return subview(from, size);
		}

		StringView subview(size_t from, size_t to) const {
			return {buffer() + from, to - from};
		}

		String substring(size_t from) const {
			return substring(from, size);
		}

		String substring(size_t from, size_t to) const {
			return StringView{buffer() + from, to - from};
		}

		void reserve(size_t i) {
			grow(i);
		}
};

template <typename T>
class Array {

	UniquePtr<T[]> ptr;
	size_t size;

	public:
		Array(decltype(nullptr)) : ptr(nullptr), size(0) {}
		Array(size_t size) : ptr(new T[size]), size(size) {}
		Array(const Array &rhs) = delete;
		Array &operator=(const Array &rhs) = delete;
		Array(Array &&rhs) = default;
		Array &operator=(Array &&rhs) = default;

		size_t length() const {
			return size;
		}
		T &operator[](size_t i) const {
			#ifdef DEBUG
			if (i >= size) [[unlikely]] {
				fatalf("index %llu is out of bounds for length %llu\n", i, size);
			}
			#endif
			return ptr[i];
		}
		T *begin() const {
			return ptr.get();
		}
		T *end() const {
			return ptr.get() + size;
		}
		explicit operator bool() const {
			return (bool)ptr && size > 0;
		}
		bool operator==(decltype(nullptr)) const {
			return ptr == nullptr;
		}
		T *data() { return ptr.get(); }
};

template <typename T>
class List;

template <typename T>
class _List_iterator;

template <typename T>
class _List_node {
	friend class List<T>;
	friend class _List_iterator<T>;

	_List_node<T> *next;
	// don't need a doubly linked list
	T value;

	public:
		_List_node(T t) : next(nullptr), value(t) {}
		_List_node(_List_node<T> *next, T t) : next(next), value(t) {}
		~_List_node() { delete next; }

		template <typename ...Types>
		_List_node(_List_node<T> *next, Types ...values) : next(next), value(values...) {}

		T &operator*() { return value; }
};

template <typename T>
class _List_iterator {
	_List_node<T> *it;

	public:
		_List_iterator(decltype(nullptr)) :it(nullptr) {}
		_List_iterator(_List_node<T> *it) : it(it) {}
		T &operator*() const { return **it; }
		_List_iterator<T> operator++(int) {
			_List_iterator<T> tmp = *this;
			it = it->next;
			return tmp;
		}
		_List_iterator<T> &operator++() {
			it = it->next;
			return *this;
		}
		bool operator!=(const _List_iterator<T> &rhs) const {
			// use reference equality of the nodes
			return (uintptr_t)it != (uintptr_t)rhs.it;
		}
};

template <typename T>
class List {
	_List_node<T> *head;
	size_t size;

	public:
		List() : head(nullptr), size(0) {}
		List(List<T> &&rhs) : head(rhs.head), size(rhs.size) { rhs.head = nullptr; }
		List &operator=(List<T> &&rhs) {
			delete head;
			head = rhs.head;
			size(rhs.size);
			rhs.head = nullptr;
		}
		~List() {
			delete head;
		}

		size_t length() const { return size; }

		template<typename ...Types>
		T &emplace_front(Types ...values) {
			// much cheaper to emplace in front
			size++;
			head = new _List_node<T>(head, values...);
			return head->value;
		}

		T &push_front(const T &value) {
			size++;
			head = new _List_node<T>(head, value);
			return head->value;
		}

		_List_iterator<T> begin() const {
			return head;
		}

		_List_iterator<T> end() const {
			return nullptr;
		}

		const T& operator[](size_t i) const {
			auto it = begin();
			for (size_t j = 0; j < i; j++) {
				it++;
			}
			return *it;
		}
};

[[maybe_unused]] static void hexdump(const void *ptr, size_t len) {
	const uint8_t *buf = (uint8_t *)ptr;
	auto rows = len / 16;
	for (size_t i = 0; i < rows; i++) {
		auto j = i * 16;
		__builtin_printf(
			"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
			buf[j], buf[j+1], buf[j+2], buf[j+3], buf[j+4], buf[j+5], buf[j+6], buf[j+7],
			buf[j+8], buf[j+9], buf[j+10], buf[j+11], buf[j+12], buf[j+13], buf[j+14], buf[j+15]
		);
	}
	buf += rows * 16;
	for (size_t i = 0; i < len % 16; i++) {
		__builtin_printf("%02X ", buf[i]);
	}
	puts("");
}
