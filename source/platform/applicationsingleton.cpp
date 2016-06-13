//---------------------------------------------------------------------------------------------------------------------
// Copyright (c) 2015-2016 librapid project. All rights reserved.
// More license information, please see LICENSE file in module root folder.
//---------------------------------------------------------------------------------------------------------------------

#include <memory>
#include <sstream>
#include <vector>

#include <windows.h>
#include <Sddl.h>

#include <rapid/exception.h>
#include <rapid/platform/applicationsingleton.h>

namespace rapid {

namespace platform {

struct LocalAdminSid {
	LocalAdminSid()
		: localAdminSID(SECURITY_MAX_SID_SIZE) {
		pSid = reinterpret_cast<SID *>(localAdminSID.data());
		DWORD cbSID = SECURITY_MAX_SID_SIZE;
		if (!::CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, pSid, &cbSID)) {
			throw Exception(::GetLastError());
		}
	}
	~LocalAdminSid() throw() {
	}
	std::vector<BYTE> localAdminSID;
	SID *pSid;
};

static HANDLE createPrivateNamespace(SECURITY_ATTRIBUTES *pSecurityAttributes,
	HANDLE boundaryDescriptor,
	std::string namespaceName) {
	auto privateNamespace = ::CreatePrivateNamespaceA(pSecurityAttributes, boundaryDescriptor, namespaceName.c_str());
	
	::LocalFree(pSecurityAttributes->lpSecurityDescriptor);

	if (!privateNamespace) {
		auto lastError = ::GetLastError();
		switch (lastError) {
		case ERROR_ALREADY_EXISTS:
			privateNamespace = ::OpenPrivateNamespaceA(boundaryDescriptor, namespaceName.c_str());
			break;
		case ERROR_ACCESS_DENIED:
			throw Exception(ERROR_ACCESS_DENIED);
			break;
		default:
			throw Exception(lastError);
		}
	}
	return privateNamespace;
}

static SECURITY_ATTRIBUTES createAdminSecurityAttributes() {
	SECURITY_ATTRIBUTES securityAttributes = { 0 };
	securityAttributes.nLength = sizeof(securityAttributes);
	securityAttributes.bInheritHandle = FALSE;

	if (!::ConvertStringSecurityDescriptorToSecurityDescriptorA("D:(A;;GA;;;BA)",
		SDDL_REVISION_1,
		&securityAttributes.lpSecurityDescriptor,
		nullptr)) {
		throw Exception(::GetLastError());
	}
	return securityAttributes;
}

struct PrivateNamespace {
	PrivateNamespace(SECURITY_ATTRIBUTES *pSecurityAttributes,
		HANDLE boundaryDescriptor,
		std::string namespaceName)
		: handle(nullptr) {
		handle = createPrivateNamespace(pSecurityAttributes, boundaryDescriptor, namespaceName);
	}

	~PrivateNamespace() throw() {
		if (handle) {
			::ClosePrivateNamespace(handle, 0);
		}
	}
	HANDLE handle;
};

struct BoundaryDescriptor {
	explicit BoundaryDescriptor(std::string boundaryName)
		: handle(nullptr) {
		handle = ::CreateBoundaryDescriptorA(boundaryName.c_str(), 0);
	}

	~BoundaryDescriptor() throw() {
		if (handle) {
			::DeleteBoundaryDescriptor(handle);
		}
	}
	HANDLE handle;
};

class ApplicationSingleton::ApplicationSingletonImpl {
public:
	static char const APP_GUID[];

	ApplicationSingletonImpl();

	~ApplicationSingletonImpl();

	bool hasInstanceRunning() const;
private:
	void makeInstance(std::string boundaryName, std::string namespaceName);
	std::unique_ptr<BoundaryDescriptor> pBoundaryDescriptor;
	std::unique_ptr<LocalAdminSid> pAdminSid;
	std::unique_ptr<PrivateNamespace> pPrivateNamespace;
	HANDLE handle;
	DWORD lastError;
};

char const ApplicationSingleton::ApplicationSingletonImpl::APP_GUID[] = {
	"C2BD6A84-F0C9-463D-A208-8F1696C8D9AF"
};

void ApplicationSingleton::ApplicationSingletonImpl::makeInstance(std::string boundaryName, 
	std::string namespaceName) {
	pBoundaryDescriptor.reset(new BoundaryDescriptor(boundaryName.c_str()));
	pAdminSid.reset(new LocalAdminSid());
	// 將本機系統管理員SID與邊界描述元聯結起來
	if (!::AddSIDToBoundaryDescriptor(&pBoundaryDescriptor->handle, pAdminSid->pSid)) {
		throw Exception();
	}
	// 只為系統管理員建立命名空間
	auto securityAttributes = createAdminSecurityAttributes();
	pPrivateNamespace.reset(new PrivateNamespace(&securityAttributes, pBoundaryDescriptor->handle, namespaceName));
	std::ostringstream ostr;
	ostr << namespaceName << "\\" << APP_GUID;
	handle = ::CreateMutexA(nullptr, FALSE, ostr.str().c_str());
}

bool ApplicationSingleton::ApplicationSingletonImpl::hasInstanceRunning() const {
	return lastError == ERROR_ALREADY_EXISTS;
}

ApplicationSingleton::ApplicationSingletonImpl::ApplicationSingletonImpl()
	: handle(nullptr)
	, lastError(ERROR_ALREADY_EXISTS) {
	makeInstance("ApplicationSingletonBoundary", "ApplicationSingleton");
	lastError = ::GetLastError();
}

ApplicationSingleton::ApplicationSingletonImpl::~ApplicationSingletonImpl() {
	if (handle) {
		::CloseHandle(handle);
	}
}

ApplicationSingleton::ApplicationSingleton()
	: pImpl(new ApplicationSingletonImpl()) {
}

ApplicationSingleton::~ApplicationSingleton() {
	
}

bool ApplicationSingleton::hasInstanceRunning() const {
	return pImpl->hasInstanceRunning();
}

}

}
