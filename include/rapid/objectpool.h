//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <memory>
#include <atomic>

#include <rapid/platform/slist.h>

namespace rapid {

template <typename T, typename DeletePolicy>
class GenericObjectPool {
public:
    class DefaultObjectDeleter {
    public:
        DefaultObjectDeleter() {
        }

        explicit DefaultObjectDeleter(std::weak_ptr<GenericObjectPool<T, DeletePolicy>*> pool)
            : pPool(pool) {
        }

        void operator()(T* pT) {
            auto pool = pPool.lock();
            if (!pool) {
				// pool�ѦҤw���ĴNdelete object!
				DeletePolicy{}(pT);
				return;
            }
			try {
				// pool�ѦҦ��ĩ�^pool��
				(*pool.get())->returnObject(std::unique_ptr<T, DefaultObjectDeleter> { pT });
				return;
			}
			catch (...) {
				// �����ҥ~!
			}
        }
    private:
        std::weak_ptr<GenericObjectPool<T, DeletePolicy>*> pPool;
    };

    using PoolablesObjectPtr = std::unique_ptr<T, DefaultObjectDeleter>;

    GenericObjectPool();

    GenericObjectPool(GenericObjectPool const &) = delete;
    GenericObjectPool& operator=(GenericObjectPool const &) = delete;

    void returnObject(PoolablesObjectPtr pObj);

    PoolablesObjectPtr borrowObject();

	void expand(uint32_t poolSize);

	void destory();

	uint16_t count() const {
		return pFreelist_.count();
	}

private:
	PoolablesObjectPtr allocateObject();
    std::shared_ptr<GenericObjectPool<T, DeletePolicy>*> pSharedThisPtr_;
	platform::SList<PoolablesObjectPtr> pFreelist_;
};

template <typename T, typename DeletePolicy>
GenericObjectPool<T, DeletePolicy>::GenericObjectPool()
    : pSharedThisPtr_(new GenericObjectPool<T, DeletePolicy>*(this)) {
	// allocate ���V�ۤv������, �ΨӦ@�ɵ��Ҧ�pooled object
}

template <typename T, typename DeletePolicy>
void GenericObjectPool<T, DeletePolicy>::expand(uint32_t poolSize) {
	for (uint32_t i = 0; i < poolSize; ++i) {
		returnObject(allocateObject());
	}
}

template <typename T, typename DeletePolicy>
void GenericObjectPool<T, DeletePolicy>::destory() {
	pSharedThisPtr_.reset();
}

template <typename T, typename DeletePolicy>
void GenericObjectPool<T, DeletePolicy>::returnObject(PoolablesObjectPtr pObj) {
	pFreelist_.enqueue(std::move(pObj));
}

template <typename T, typename DeletePolicy>
typename GenericObjectPool<T, DeletePolicy>::PoolablesObjectPtr
GenericObjectPool<T, DeletePolicy>::borrowObject() {
	PoolablesObjectPtr tmp;
	if (pFreelist_.tryDequeue(tmp)) {
		return tmp;
	}
	return allocateObject();
}

template <typename T, typename DeletePolicy>
typename GenericObjectPool<T, DeletePolicy>::PoolablesObjectPtr 
GenericObjectPool<T, DeletePolicy>::allocateObject() {
	// �t�m�@��pooled object���ɭԴN�ǤJdeleter, ������object destructor�k�٪���!
	return PoolablesObjectPtr(new T(), DefaultObjectDeleter {
		std::weak_ptr<GenericObjectPool<T, DeletePolicy>*>{ pSharedThisPtr_ }
	});
}

template <typename T>
using ObjectPool = GenericObjectPool<T, std::default_delete<T>>;

}
