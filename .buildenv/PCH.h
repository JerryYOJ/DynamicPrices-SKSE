#pragma once

// This file is required.

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

using namespace std::literals;
namespace logger = SKSE::log;

template <typename T>
class SINGLETON {
public:
    static T* getInstance() {
        static T instance; // Create instance of T
        return std::addressof(instance);
    }

protected:
    SINGLETON() = default;
    ~SINGLETON() = default;

    // Delete copy and move constructors and assignment operators
    SINGLETON(const SINGLETON&) = delete;
    SINGLETON(SINGLETON&&) = delete;
    SINGLETON& operator=(const SINGLETON&) = delete;
    SINGLETON& operator=(SINGLETON&&) = delete;
};

template <typename T>
class SingletonWithData : public SINGLETON<T> {
public:
    union variable {
        int inum;
        float fnum;
        void* ptr;
    };
    static std::map<std::string, variable>& getData() {
        static std::map<std::string, variable> storage;
        return storage;
    }
};

template <typename T>
class HookTemplate {
protected:
    HookTemplate() = delete;
    HookTemplate(const HookTemplate&) = delete;
    HookTemplate(HookTemplate&&) = delete;
    ~HookTemplate() = delete;

    HookTemplate& operator=(const HookTemplate&) = delete;
    HookTemplate& operator=(HookTemplate&&) = delete;
};

class VmCallback : public RE::BSScript::IStackCallbackFunctor
{
public:
    using OnResult = std::function<void(const RE::BSScript::Variable&)>;

    static auto New(const OnResult& a_callback)
    {
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> res;
        res.reset(new VmCallback(a_callback));
        return res;
    }
private:
    VmCallback(const OnResult& onResult_) :
        onResult(onResult_) {}

    void operator()(RE::BSScript::Variable result) override { onResult(result); }
    bool CanSave() const override { return false; }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

    const OnResult onResult;
};

#ifdef SKYRIM_AE
#	define OFFSET(se, ae) ae
#	define OFFSET_3(se, ae, vr) ae
#elif SKYRIMVR
#	define OFFSET(se, ae) se
#	define OFFSET_3(se, ae, vr) vr
#else
#	define OFFSET(se, ae) se
#	define OFFSET_3(se, ae, vr) se
#endif