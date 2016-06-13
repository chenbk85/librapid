//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <queue>

// Reference : http://llvm.org/docs/doxygen/html/PriorityQueue_8h_source.html
template <typename T, class Container = std::vector<T>, class Compare = std::less<typename Container::value_type>>
class SearchPriorityQueue : public std::priority_queue<T, Container, Compare> {
public:
	typedef typename std::priority_queue<T, Container, Compare>::container_type::const_iterator const_iterator;
	typedef typename std::priority_queue<T, Container, Compare>::container_type::iterator iterator;

	const_iterator find(T const &val) const {
		auto first = this->c.cbegin();
		auto last = this->c.cend();
		while (first != last) {
			if (*first == val) return first;
			++first;
		}
		return last;
	}

	iterator find(T const &val) {
		auto first = this->c.begin();
		auto last = this->c.end();
		while (first != last) {
			if (*first == val) return first;
			++first;
		}
		return last;
	}

	void erase_one(const T &t) {
		// Linear-search to find the element.
		typename Container::size_type i =
			std::find(this->c.begin(), this->c.end(), t) - this->c.begin();

		// Logarithmic-time heap bubble-up.
		while (i != 0) {
			typename Container::size_type parent = (i - 1) / 2;
			this->c[i] = this->c[parent];
			i = parent;
		}

		// The element we want to remove is now at the root, so we can use
		// priority_queue's plain pop to remove it.
		this->pop();
	}

	void reheapify() {
		std::make_heap(this->c.begin(), this->c.end(), this->comp);
	}

	void clear() {
		this->c.clear();
	}

	Container const & container() const {
		return c; 
	}
};
