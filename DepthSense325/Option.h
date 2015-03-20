#pragma once

namespace mobamas {

template<typename T>
class Option {
public:
	explicit Option(T const& value) :
		is_engaged_(true), value_(value) {}

	operator bool() const { return is_engaged_; }

	T const& operator*() const { return value_; }
	T& operator*() { return value_; }

	static Option<T> None() { return Option(); }
private:
	Option() : is_engaged_(false), value_() {}


	bool is_engaged_;
	T value_;
};

}