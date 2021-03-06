#ifndef NETWORKREADBUFFER_H
#define NETWORKREADBUFFER_H

#include "readbuffer.h"

namespace XLib
{
    class NetworkReadBuffer : public Buffer
    {
      public:
        NetworkReadBuffer(data_t data         = nullptr,
                          safesize_t maxSize  = 0,
                          safesize_t readBits = 0);

        bool readBit();
        void pos(safesize_t toBit = 0);

        template <typesize_t typesize = type_array>
        auto readVar()
        {
            if constexpr (typesize == type_array)
            {
                static_assert("Can't read as type_array");
            }
            else
            {
                g_v_t<typesize> var {};

                for (size_t i = 0; i < sizeof(g_v_t<typesize>) * 8; i++)
                {
                    if (readBit())
                    {
                        var += view_as<g_v_t<typesize>>(1)
                               << view_as<g_v_t<typesize>>(i);
                    }
                }

                return var;
            }
        }

      private:
        safesize_t _read_bits {};
    };
};

#endif
