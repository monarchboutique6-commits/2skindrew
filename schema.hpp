#pragma once
#include <cstdint>

namespace schema {
    constexpr std::ptrdiff_t m_pWeaponServices = 0x11E0;
    constexpr std::ptrdiff_t m_hMyWeapons = 0x48;

    constexpr std::ptrdiff_t m_AttributeManager = 0x1180;
    constexpr std::ptrdiff_t m_nFallbackPaintKit = 0x1658;
    constexpr std::ptrdiff_t m_nFallbackSeed = 0x165C;
    constexpr std::ptrdiff_t m_flFallbackWear = 0x1660;
    constexpr std::ptrdiff_t m_nFallbackStatTrak = 0x1664;

    constexpr std::ptrdiff_t m_Item = 0x50;

    constexpr std::ptrdiff_t m_iItemDefinitionIndex = 0x1BA;
    constexpr std::ptrdiff_t m_iItemIDHigh = 0x1D0;
    constexpr std::ptrdiff_t m_iItemIDLow = 0x1D4;
    constexpr std::ptrdiff_t m_iAccountID = 0x1D8;
    constexpr std::ptrdiff_t m_bInitialized = 0x1E8;
}