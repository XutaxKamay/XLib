#ifndef DETOUR_H
#define DETOUR_H

#include "types.h"

#ifdef WINDOWS
enum calling_conventions_t
{
    cc_fastcall,
    cc_stdcall,
    cc_cdecl
};
#endif

namespace XLib
{
#ifdef _WIN32
    template < calling_conventions_t TCC,
               typename TRetType,
               typename... TArgs >
#else
    template < typename TRetType, typename... TArgs >
#endif
    /**
     * @brief Detour
     * This class permits to hook any functions inside the current
     * process. Detour is a method to hook functions. It works generally
     * by placing a JMP instruction on the start of the function. This one
     * works by copying a small portion of opcodes that the JMP
     * instruction override, disassemble them and search the closest
     * address to the function address in order to allocate memory, so we
     * can use a relative JMP instruction. If the disassembled
     * instructions contains addresses that needs relocation since they're
     * relative most of the time, it will automatically patch them by
     * checking if it's pointing to the valid address and memory or not.
     */
    class Detour
    {
      private:
        /* This case is only for windows 32 bits program */
#ifdef _WIN32
        /**
         * @brief GenCallBackFuncType
         *
         * @return auto
         */
        constexpr auto GenCallBackFuncType();
        /**
         * @brief GenCallBackFuncType
         *
         * @return auto
         */
        constexpr auto GenNewFuncType();

      public:
        using cbfunc_t = typename decltype( GenCallBackFuncType() )::type;
        using func_t   = typename decltype( GenNewFuncType() )::type;

#else /* Otherwhise it should be always the same convention */
        using cbfunc_t = TRetType ( * )( TArgs... );
        using func_t   = cbfunc_t;
#endif

      public:
      private:
        /**
         * @brief _callBackFunc
         * The call back function permits to call the original function
         * inside the new function.
         */
        cbfunc_t _callBackFunc {};
        /**
         * @brief _newFunc
         * Pointer to the new function.
         */
        func_t _newFunc {};
    };

    /**
     * On windows 32 bits programs, the calling convention needs to be
     * specified in order to call back the original function when
     * detoured. It is not needed on others architectures/os because the
     * calling conventions are always the same (64 bits is fastcall on
     * Windows, same for Linux), and 32 bits on Linux the parameters are
     * always pushed to the stack.
     */
#ifdef _WIN32
    template < calling_conventions_t TCC,
               typename TRetType,
               typename... TArgs >
    constexpr auto Detour< TCC, TRetType, TArgs... >::GenCallBackFuncType()
    {
        if constexpr ( TCC == cc_fastcall )
        {
            /* thisptr - EDX, stack params */
            return type_wrapper< TRetType( __thiscall* )( ptr_t,
                                                          TArgs... ) >;
        }
        else if constexpr ( TCC == cc_stdcall )
        {
            return type_wrapper< TRetType( __stdcall* )( TArgs... ) >;
        }
        else
        {
            return type_wrapper< TRetType ( * )( TArgs... ) >;
        }
    }

    template < calling_conventions_t TCC,
               typename TRetType,
               typename... TArgs >
    constexpr auto Detour< TCC, TRetType, TArgs... >::GenNewFuncType()
    {
        if constexpr ( TCC == cc_fastcall )
        {
            /* EDX, ECX, stack params */
            return type_wrapper< TRetType(
              __fastcall* )( ptr_t, ptr_t, TArgs... ) >;
        }
        else if constexpr ( TCC == cc_stdcall )
        {
            return type_wrapper< TRetType( __stdcall* )( TArgs... ) >;
        }
        else
        {
            return type_wrapper< TRetType ( * )( TArgs... ) >;
        }
    }
#endif
}

#endif // DETOUR_H