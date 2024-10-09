#pragma once
#include <cstdint>
#include <type_traits>
#include <typeinfo>
#include <tuple>
#include <iostream>

template<typename D, typename R, typename... Args>
struct ThunkImpl {};

template<typename D, typename R, typename... Args>
struct ThunkImpl<D, R(*)(Args...)>
{
    using Tuple = std::tuple<Args...>;
    static constexpr auto size = sizeof...(Args);
    
    // Gather a single argument from the guest environment. If the first argument is a
    // jit instance pointer, then we'll inject it from the current env instead of the
    // argument pack provided by the guest.
    template <typename T> static T get(int &i, int &r, int &f, uintptr_t &sp, Dynarmic::A64::Jit *jit) {
        // Do we inject a jit instance into this index of the argument list?
        int idx = i++;
        if (idx == 0) {
            if constexpr (std::is_same_v<T, Dynarmic::A64::Jit *>)
                return jit;
        }

        // Get the index of the register per-type
        int idx_reg = [&](){
            if constexpr (std::is_floating_point_v<T>) {
                return f++;
            } else {
                return r++;
            }
        }();

        // If we exausted the available registers, then we have to
        // fetch the instruction from the stack.
        if (idx_reg >= 7) {
            sp -= 8;
            return *(T*)sp;
        } else {
            if constexpr (std::is_floating_point_v<T>) {
                return reinterpret_cast<T>(jit->GetVector(f));
            } else {
                return reinterpret_cast<T>(jit->GetRegister(r));
            }
        }
    }

    struct BraceCall
    {
        R ret;
        inline BraceCall(Args... args) : ret( D::template bridge_impl<Args...>(args...) ) { };
    };

    struct BraceCallVoid
    {
        inline BraceCallVoid(Args... args) { D::template bridge_impl<Args...>(args...); };
    };

    __attribute__((noinline)) static void bridge(Dynarmic::A64::Jit *jit)
    {
        // Store the return address of the function
        uintptr_t addr_next = jit->GetRegister(30);

        // Gather arguments from the guest environment and pass them to the wrapped
        // function.
        uintptr_t sp = jit->GetSP();
        int float_reg_cnt = 0;
        int int_reg_cnt = 0;
        int reg_cnt = 0;
        if constexpr (std::is_void_v<R>) {
            BraceCallVoid { get<Args>(reg_cnt, int_reg_cnt, float_reg_cnt, sp, jit)... };
        } else {
            R ret = BraceCall { get<Args>(reg_cnt, int_reg_cnt, float_reg_cnt, sp, jit)... }.ret;
            if (std::is_floating_point_v<R>) {
                double ret_cast = ret;
                uint32_t *alias = (uint32_t*)&ret_cast;
                jit->SetVector(0, Dynarmic::A64::Vector{alias[0], alias[1]});
            }
            else {
                jit->SetRegister(0, ret);
            }
        }
        
        // Jump back to the host :)
        jit->SetPC(addr_next);
    }
};

template<typename D, typename R, typename... Args>
struct ThunkImpl<D, R(*)(Args...) noexcept>: ThunkImpl<D, R, Args...>
{ };

template<auto Def, typename PFN>
struct Thunk : ThunkImpl<Thunk<Def, PFN>, PFN>
{
public:
    static inline PFN func;
    static inline const char* symname = NULL;

    template<typename... Args>
    static auto bridge_impl(Args... args)
    {
#ifndef NDEBUG
        if (symname) {
            std::cout << symname << "(";
            ((std::cout << args << ", "), ...);
            std::cout << ")\n";
        }
#endif
        return func(args...);
    }
};

template <auto F, class T = Thunk<F, decltype(F)>>
dynarec_import gen_wrapper(const char *symname)
{
    // Setup the static fields
    T::func = F;
    T::symname = symname;
    uintptr_t f = (uintptr_t)T::bridge;
    uint32_t *f_hilo = reinterpret_cast<uint32_t*>(&f); /* alias the function ptr */

    // Setup the trampoline
    return (dynarec_import){
        .symbol = symname,
        .func = (uintptr_t)T::bridge,
        //   The trampoline works by loading the address of our wrapper into x17
        // and then calling a SVC Handler that takes care of gathering the
        // arguments from guest and passing them to the host function as needed
        // injecting the jit context pointer if requested by the wrapped function.
        .trampoline = {
            0x10000071,            /*      adr x17, ptr   */
            0xF9400230,            /*      ldr x16, [x17] */
            0xD4000021,            /*      svc 0x2        */
                                   /* ptr:                */
            f_hilo[0],             /*     .word hi-ptr    */
            f_hilo[1],             /*     .word lo-ptr    */
        }
    };
}