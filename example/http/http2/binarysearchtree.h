//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cassert>
#include <memory>

template <typename T>
class BinarySearchTree {
public:
	struct Node {
		Node(std::shared_ptr<Node> lft,
			T val,
			std::shared_ptr<Node> rgt)
			: left(lft)
			, value(val)
			, right(rgt) {
		}
		std::shared_ptr<Node> left;
		T value;
		std::shared_ptr<Node> right;
	};

	template <typename F>
	static void walk(BinarySearchTree<T> const & tree, F f) {
		if (!tree.isEmpty()) {
			walk(tree.getLeftTree(), f);
			f(tree.getRoot());
			walk(tree.getRightTree(), f);
		}
	}

	BinarySearchTree()
		: BinarySearchTree(nullptr) {
	}

	BinarySearchTree(BinarySearchTree const & lft, T val, BinarySearchTree const & rgt)
		: root(std::make_shared<Node>(lft.root, val, rgt.root)) {
		assert(lft.isEmpty() || lft.getRoot() < val);
		assert(rgt.isEmpty() || val < rgt.getRoot());
	}

	T getRoot() const {
		return root->value;
	}

	bool isEmpty() const {
		return !root;
	}

	BinarySearchTree getLeftTree() const {
		return BinarySearchTree(root->left);
	}

	BinarySearchTree getRightTree() const {
		return BinarySearchTree(root->right);
	}

	std::shared_ptr<Node> insert(std::shared_ptr<Node> &parrent, T x) {
		std::shared_ptr<Node> node = std::make_shared<Node>(nullptr, x, nullptr);
		std::shared_ptr<Node> parent;

		if (isEmpty()) {
			parrent.reset();
			parrent = node;
		} else {
			std::shared_ptr<Node> current = parrent;
			while (current) {
				parent = current;
				current = node->value < current->value ? current->left : current->right;
			}
			if (node->value < parent->value) {
				parent->left = node;
			} else {
				parent->right = node;
			}
		}
		
		return node;
	}

	std::shared_ptr<Node> insert(T x) {
		return insert(root, x);
	}

	std::shared_ptr<Node> find(T x) const {
		auto current = root;
		while (current) {
			if (current->value == x) {
				break;
			}
			if (current->value < x) {
				current = current->left;
			} else {
				current = current->right;
			}
		}
		return current;
	}

	bool containsKey(T k) const {
		return find(k) != nullptr;
	}

private:
	explicit BinarySearchTree(std::shared_ptr<Node> node)
		: root(node) {
	}
	std::shared_ptr<Node> root;
};

