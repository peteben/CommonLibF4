#include "PCH.h"
#include <RE/msvc/functional.h>
#include <RE/Bethesda/FunctionArgs.h>

/* 
Functions for calling Papyrus functions from C++
NG changed the parameters for the DispatchMethodCall function,
so we need to call the function differently for OG/VR and NG

Usage: in Papyrus, use 'RegsiterForExternalEvent'
in C++, call 'SendPapyrusExternalEvent'
*/

template <class F>
using BSTThreadScrapFunction = std::function<F>;        // Used by NG for DispatchMethod calls

template <class F>
using BSTThreadScrapFunctionOG = RE::msvc::function<F>; // Used by OG for same

namespace Papyrus {
namespace detail {

// NG can use the current IVirtualMachine::DispatchMethodCall as is.
// OG/VR, we need to call the function with different args.


bool DispatchMethodCallOG(
    RE::BSScript::IVirtualMachine* vm,
    std::uint64_t a_objHandle,
    const RE::BSFixedString& a_objName,
    const RE::BSFixedString& a_funcName,
    const BSTThreadScrapFunctionOG<bool(RE::BSScrapArray<RE::BSScript::Variable>&)>& a_arguments,
    const RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>& a_callback) {

    // Get function address out of the Vtable
	auto VMvtbl = RE::BSScript::Internal::VirtualMachine::VTABLE;
    REL::ID VMvtblID = VMvtbl[0];
    UINT64 DispatchMethodptr;

    void** pt = (void**)VMvtblID.address();
    DispatchMethodptr = (size_t)pt[46];

    using func_t = decltype(&DispatchMethodCallOG);
    REL::Relocation<func_t> func{ DispatchMethodptr };

    return func(vm, a_objHandle, a_objName, a_funcName, a_arguments, a_callback);
    }

// Call all the Papyrus functions registered with a_eventName

template <class... Args>
static void SendPapyrusExternalEvent(std::string a_eventName, Args... _args) {
    auto const papyrus = F4SE::GetPapyrusInterface();
    auto* vm = RE::GameVM::GetSingleton()->GetVM().get();
    

    if (REL::Module::IsNG()) {
		struct PapyrusEventData {
			RE::BSScript::IVirtualMachine* vm;
			RE::BSScrapArray<RE::BSScript::Variable> argsPacked;
			};
		PapyrusEventData evntData;

		evntData.argsPacked = PackVariables(_args...);
		evntData.vm = vm;

        papyrus->GetExternalEventRegistrations(
            a_eventName, vm,
            [](uint64_t handle, const char* scriptName, const char* callbackName, void* dataPtr) {
            PapyrusEventData* d = (PapyrusEventData*)dataPtr;
            d->vm->DispatchMethodCall(handle, scriptName, callbackName,
                [&](RE::BSScrapArray<RE::BSScript::Variable>& a_out) {
                a_out = d->argsPacked;
                return true;
                },
                nullptr);
            }
        );
        }
    else {
        struct PapyrusEventData {
            RE::BSScript::IVirtualMachine* vm;
            BSTThreadScrapFunctionOG<bool(RE::BSScrapArray<RE::BSScript::Variable>&)> scrapFunc;
            };
        PapyrusEventData evntData;

        evntData.scrapFunc = (FunctionArgs{ vm, _args... }).get();
        evntData.vm = vm;

        papyrus->GetExternalEventRegistrations(
            a_eventName, &evntData,
            [](uint64_t handle, const char* scriptName, const char* callbackName, void* dataPtr) {
                PapyrusEventData* d = static_cast<PapyrusEventData*>(dataPtr);
                DispatchMethodCallOG(d->vm, handle, scriptName, callbackName, d->scrapFunc, nullptr);
                }
            );
        }
    }
}
}
