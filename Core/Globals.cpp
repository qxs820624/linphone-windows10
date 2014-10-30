#include <hstring.h>
#include <memory>
#include <activation.h>
#include <wrl\module.h>
#include <crtdbg.h>
#include "Globals.h"
#include "ApiLock.h"
#include "LinphoneCoreFactory.h"
#include "LinphoneCore.h"
#include "BackgroundModeLogger.h"
#include "CallController.h"

using namespace Linphone::Core;
using namespace Windows::Foundation;
using namespace Windows::Phone::Media::Capture;

// Maximum number of characters required to contain the string version of an unsigned integer
#define MAX_CHARS_IN_UINT_AS_STRING ((sizeof(unsigned int) * 4) + 1)

const LPCWSTR Globals::noOtherBackgroundProcessEventName = L"Linphone.noOtherBackgroundProcess";
const LPCWSTR Globals::uiDisconnectedEventName = L"Linphone.uiDisconnected.";
const LPCWSTR Globals::backgroundProcessReadyEventName = L"Linphone.backgroundProcessReady.";
Globals^ Globals::singleton = nullptr;

void Globals::StartServer(const Platform::Array<Platform::String^>^ outOfProcServerClassNames)
{
    // Make sure only one API call is in progress at a time
	API_LOCK;

    std::unique_ptr<PFNGETACTIVATIONFACTORY[]> activationFactoryCallbacks;
    std::unique_ptr<HSTRING[]> hOutOfProcServerClassNames;

    if (outOfProcServerClassNames == nullptr)
    {
        throw ref new Platform::InvalidArgumentException(L"outOfProcServerClassNames cannot be null");
    }
    unsigned int numOutOfProcServerClassNames = outOfProcServerClassNames->Length;

    if (this->started)
    {
		API_UNLOCK;
        return; // Nothing more to be done
    }

    // Initialize the structures required to register the out-of-proc server classes
    activationFactoryCallbacks.reset(new PFNGETACTIVATIONFACTORY[numOutOfProcServerClassNames]);        
    hOutOfProcServerClassNames.reset(new HSTRING[numOutOfProcServerClassNames]);
    for(unsigned int index = 0; index < numOutOfProcServerClassNames; index++)
    {
        activationFactoryCallbacks[index] = &(Microsoft::WRL::Details::ActivationFactoryCallback<Microsoft::WRL::OutOfProcDisableCaching>);
        hOutOfProcServerClassNames[index] = reinterpret_cast<HSTRING>(outOfProcServerClassNames[index]);
    }

    // Register the out-of-proc server classes
    HRESULT hr = ::RoRegisterActivationFactories(
        hOutOfProcServerClassNames.get(), activationFactoryCallbacks.get(), numOutOfProcServerClassNames, &this->serverRegistrationCookie);
    if (FAILED(hr))
    {
        throw ref new Platform::COMException(hr, L"Unable to start server");
    }

    // Set an event that indicates that the background process is ready.
    this->backgroundReadyEvent = ::CreateEventEx(
        NULL,
        Globals::GetBackgroundProcessReadyEventName(Globals::GetCurrentProcessId())->Data(),
        CREATE_EVENT_INITIAL_SET | CREATE_EVENT_MANUAL_RESET,
        EVENT_ALL_ACCESS);
    if (this->backgroundReadyEvent == NULL)
    {
        // Something went wrong
        DWORD dwErr = ::GetLastError();
        hr = HRESULT_FROM_WIN32(dwErr);
        throw ref new Platform::COMException(hr, L"An error occurred trying to create an event that indicates that the background process is ready");
    }

    // Set the event
    BOOL success = ::SetEvent(this->backgroundReadyEvent);
    if (success == FALSE)
    {
        DWORD dwErr = ::GetLastError();
        hr = HRESULT_FROM_WIN32(dwErr);
        throw ref new Platform::COMException(hr, L"An error occurred trying to set an event that indicates that the background process is ready");
    }

    this->started = true;
	API_UNLOCK;
}

unsigned int Globals::GetCurrentProcessId()
{
    return ::GetCurrentProcessId();
}

Platform::String^ Globals::GetUiDisconnectedEventName(unsigned int backgroundProcessId)
{
    WCHAR backgroundProcessIdString[MAX_CHARS_IN_UINT_AS_STRING];
    if (swprintf_s<_countof(backgroundProcessIdString)>(backgroundProcessIdString, L"%u", backgroundProcessId) < 0)
        throw ref new Platform::FailureException(L"Could not create string version of background process id");

    auto eventName = ref new Platform::String(Globals::uiDisconnectedEventName) + ref new Platform::String(backgroundProcessIdString);
    return eventName;
}

Platform::String^ Globals::GetBackgroundProcessReadyEventName(unsigned int backgroundProcessId)
{
    WCHAR backgroundProcessIdString[MAX_CHARS_IN_UINT_AS_STRING];
    if (swprintf_s<_countof(backgroundProcessIdString)>(backgroundProcessIdString, L"%u", backgroundProcessId) < 0)
        throw ref new Platform::FailureException(L"Could not create string version of background process id");

    auto eventName = ref new Platform::String(Globals::backgroundProcessReadyEventName) + ref new Platform::String(backgroundProcessIdString);
    return eventName;
}

Globals^ Globals::Instance::get()
{
    if (Globals::singleton == nullptr)
    {
        // Make sure only one API call is in progress at a time
		API_LOCK;

        if (Globals::singleton == nullptr)
        {
            Globals::singleton = ref new Globals();
        }
        // else: some other thread has created an instance of the call controller

		API_UNLOCK;
    }

    return Globals::singleton;
}

LinphoneCoreFactory^ Globals::LinphoneCoreFactory::get()
{
	if (this->linphoneCoreFactory == nullptr)
    {
        // Make sure only one API call is in progress at a time
		API_LOCK;

        if (this->linphoneCoreFactory == nullptr)
        {
            this->linphoneCoreFactory = ref new Linphone::Core::LinphoneCoreFactory();
        }

		API_UNLOCK;
    }

	return this->linphoneCoreFactory;
}

Linphone::Core::LinphoneCore^ Globals::LinphoneCore::get()
{
	return this->linphoneCoreFactory->LinphoneCore;
}

Linphone::Core::CallController^ Globals::CallController::get()
{
	if (this->callController == nullptr) 
    { 
        // Make sure only one API call is in progress at a time
		API_LOCK;
 
        if (this->callController == nullptr) 
        { 
			this->callController = ref new Linphone::Core::CallController(); 
        } 
        // else: some other thread has created an instance of the call controller 

		API_UNLOCK;
    } 
 
    return this->callController; 
}

Linphone::Core::BackgroundModeLogger^ Globals::BackgroundModeLogger::get()
{
	if (this->backgroundModeLogger == nullptr)
	{
		// Make sure only one API call is in progress at a time
		API_LOCK;

		if (this->backgroundModeLogger == nullptr)
		{
			this->backgroundModeLogger = ref new Linphone::Core::BackgroundModeLogger();
		}

		API_UNLOCK;
	}

	return this->backgroundModeLogger;
}

Mediastreamer2::WP8Video::IVideoRenderer^ Globals::VideoRenderer::get()
{
	return this->videoRenderer;
}

void Globals::VideoRenderer::set(Mediastreamer2::WP8Video::IVideoRenderer^ value)
{
	API_LOCK;
	this->videoRenderer = value;
	API_UNLOCK;
}

Globals::Globals() :
    started(false),
    serverRegistrationCookie(NULL),
	linphoneCoreFactory(nullptr),
	backgroundModeLogger(nullptr),
    noOtherBackgroundProcessEvent(NULL),
    backgroundReadyEvent(NULL)
{
    {
        WCHAR szBuffer[256];
        swprintf_s<ARRAYSIZE(szBuffer)>(szBuffer, L"[Globals::Globals] => VoIP background process with id %d starting up\n", this->GetCurrentProcessId());
        ::OutputDebugString(szBuffer);
    }

    // Create an event that indicates if any other VoIP background exits or not
    this->noOtherBackgroundProcessEvent = ::CreateEventEx(NULL, Globals::noOtherBackgroundProcessEventName, CREATE_EVENT_INITIAL_SET | CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
    if (this->noOtherBackgroundProcessEvent == NULL)
    {
        // Something went wrong
        DWORD dwErr = ::GetLastError();
        HRESULT hr = HRESULT_FROM_WIN32(dwErr);
        throw ref new Platform::COMException(hr, L"An error occurred trying to create an event that indicates if the background process exists or not");
    }

    // Wait for up to 30 seconds for the event to become set - if another instance of this process exists, this event would be in the reset state
    DWORD reason = ::WaitForSingleObjectEx(this->noOtherBackgroundProcessEvent, 30 * 1000, FALSE);
    _ASSERT(reason != WAIT_FAILED); // We don't care about any of the other reasons why WaitForSingleObjectEx returned
    if (reason == WAIT_TIMEOUT)
    {
        throw ref new Platform::FailureException(L"Another instance of the VoIP background process exists and that process did not exit within 30 seconds. Cannot continue.");
    }

    // Reset the event to indicate that there is a VoIP background process
    BOOL success = ::ResetEvent(this->noOtherBackgroundProcessEvent);
    if (success == FALSE)
    {
        // Something went wrong
        DWORD dwErr = ::GetLastError();
        HRESULT hr = HRESULT_FROM_WIN32(dwErr);
        throw ref new Platform::COMException(hr, L"An error occurred trying to reset the event that indicates if the background process exists or not");
    }
}

Globals::~Globals()
{
    // The destructor of this singleton object is called when the process is shutting down.

    // Before shutting down, make sure the UI process is not connected
    HANDLE uiDisconnectedEvent = ::OpenEvent(EVENT_ALL_ACCESS, FALSE, Globals::GetUiDisconnectedEventName(Globals::GetCurrentProcessId())->Data());
    if (uiDisconnectedEvent != NULL)
    {
        // The event exists - wait for it to get signaled (for a maximum of 30 seconds)
        DWORD reason = ::WaitForSingleObjectEx(uiDisconnectedEvent, 3 * 1000, FALSE);
        //_ASSERT(reason != WAIT_FAILED); // We don't care about any of the other reasons why WaitForSingleObjectEx returned
    }

    // At this point, the UI is no longer connected to the background process.
    // It is possible that the UI now reconnects to the background process - this would be a bug,
    // and we should exit the background process anyway.

    // Unset the event that indicates that the background process is ready
    BOOL success;
    if (this->backgroundReadyEvent != NULL)
    {
        success = ::ResetEvent(this->backgroundReadyEvent);
        _ASSERT(success);

        ::CloseHandle(this->backgroundReadyEvent);
        this->backgroundReadyEvent = NULL;
    }

    // Unregister the activation factories for out-of-process objects hosted in this process
    if (this->started)
    {
        RoRevokeActivationFactories(this->serverRegistrationCookie);
	    this->started = false;
    }

    // Set the event that indicates that no instance of the VoIP background process exists
    if (this->noOtherBackgroundProcessEvent != NULL)
    {
        success = ::SetEvent(this->noOtherBackgroundProcessEvent);
        _ASSERT(success);

        ::CloseHandle(this->noOtherBackgroundProcessEvent);
        this->noOtherBackgroundProcessEvent = NULL;
    }

    {
        WCHAR szBuffer[256];
        swprintf_s<ARRAYSIZE(szBuffer)>(szBuffer, L"[Globals::~Globals] => VoIP background process with id %d shutting down\n", this->GetCurrentProcessId());
        ::OutputDebugString(szBuffer);
    }
}
