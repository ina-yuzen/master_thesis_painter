#pragma once

namespace mobamas {

template<typename T>
class Option {
public:
	explicit Option(T const& value) :
		is_engaged_(true), value_(value) {}

	operator bool() const { return is_engaged_; }

	// "The behavior is undefined if *this is in disengaged state." (experimental::optional)
	T const& operator*() const { return value_; }
	T& operator*() { return value_; }

	void Clear() { is_engaged_ = false; }
	void Reset(T const& new_value) { 
		is_engaged_ = true;
		value_ = new_value; 
	}

	static Option<T> None() { return Option(); }
private:
	Option() : is_engaged_(false), value_() {}


	bool is_engaged_;
	T value_;
};

}